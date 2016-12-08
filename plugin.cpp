#ifdef __APPLE__
#define _DARWIN_UNLIMITED_STREAMS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>
#include <sys/types.h>
#include <sys/wait.h>
#include "mvt.hpp"
#include "plugin.hpp"
#include "projection.hpp"
#include "geometry.hpp"

extern "C" {
#include "jsonpull/jsonpull.h"
}

#include "write_json.hpp"
#include "read_json.hpp"

struct writer_arg {
	int write_to;
	mvt_layer *layer;
	unsigned z;
	unsigned x;
	unsigned y;
};

void *run_writer(void *a) {
	writer_arg *wa = (writer_arg *) a;

	// XXX worry about SIGPIPE?

	FILE *fp = fdopen(wa->write_to, "w");
	if (fp == NULL) {
		perror("fdopen (pipe writer)");
		exit(EXIT_FAILURE);
	}

	layer_to_geojson(fp, *(wa->layer), wa->z, wa->x, wa->y, false, false);

	if (fclose(fp) != 0) {
		perror("fclose output to filter");
		exit(EXIT_FAILURE);
	}

	return NULL;
}

// XXX deduplicate
static std::vector<mvt_geometry> to_feature(drawvec &geom) {
	std::vector<mvt_geometry> out;

	for (size_t i = 0; i < geom.size(); i++) {
		out.push_back(mvt_geometry(geom[i].op, geom[i].x, geom[i].y));
	}

	return out;
}

mvt_layer parse_layer(int fd, unsigned z, unsigned x, unsigned y, mvt_layer const &olayer) {
	mvt_layer ret;
	ret.name = olayer.name;
	ret.version = olayer.version;
	ret.extent = olayer.extent;

	FILE *f = fdopen(fd, "r");
	if (f == NULL) {
		perror("fdopen filter output");
		exit(EXIT_FAILURE);
	}
	json_pull *jp = json_begin_file(f);

	while (1) {
		json_object *j = json_read(jp);
		if (j == NULL) {
			if (jp->error != NULL) {
				fprintf(stderr, "Filter output:%d: %s\n", jp->line, jp->error);
				if (jp->root != NULL) {
					json_context(jp->root);
				}
				exit(EXIT_FAILURE);
			}

			json_free(jp->root);
			break;
		}

		json_object *type = json_hash_get(j, "type");
		if (type == NULL || type->type != JSON_STRING) {
			continue;
		}
		if (strcmp(type->string, "Feature") != 0) {
			continue;
		}

		json_object *geometry = json_hash_get(j, "geometry");
		if (geometry == NULL) {
			fprintf(stderr, "Filter output:%d: filtered feature with no geometry\n", jp->line);
			json_context(j);
			json_free(j);
			exit(EXIT_FAILURE);
		}

		json_object *properties = json_hash_get(j, "properties");
		if (properties == NULL || (properties->type != JSON_HASH && properties->type != JSON_NULL)) {
			fprintf(stderr, "Filter output:%d: feature without properties hash\n", jp->line);
			json_context(j);
			json_free(j);
			exit(EXIT_FAILURE);
		}

		json_object *geometry_type = json_hash_get(geometry, "type");
		if (geometry_type == NULL) {
			fprintf(stderr, "Filter output:%d: null geometry (additional not reported)\n", jp->line);
			json_context(j);
			exit(EXIT_FAILURE);
		}

		if (geometry_type->type != JSON_STRING) {
			fprintf(stderr, "Filter output:%d: geometry type is not a string\n", jp->line);
			json_context(j);
			exit(EXIT_FAILURE);
		}

		json_object *coordinates = json_hash_get(geometry, "coordinates");
		if (coordinates == NULL || coordinates->type != JSON_ARRAY) {
			fprintf(stderr, "Filter output:%d: feature without coordinates array\n", jp->line);
			json_context(j);
			exit(EXIT_FAILURE);
		}

		int t;
		for (t = 0; t < GEOM_TYPES; t++) {
			if (strcmp(geometry_type->string, geometry_names[t]) == 0) {
				break;
			}
		}
		if (t >= GEOM_TYPES) {
			fprintf(stderr, "Filter output:%d: Can't handle geometry type %s\n", jp->line, geometry_type->string);
			json_context(j);
			exit(EXIT_FAILURE);
		}

		drawvec dv;
		parse_geometry(t, coordinates, dv, VT_MOVETO, "Filter output", jp->line, j);
		if (mb_geometry[t] == VT_POLYGON) {
			dv = fix_polygon(dv);
		}

		// Scale and offset geometry from global to tile
		for (size_t i = 0; i < dv.size(); i++) {
			long long scale = 1LL << (32 - z);
			dv[i].x = std::round((dv[i].x - scale * x) * olayer.extent / (double) scale);
			dv[i].y = std::round((dv[i].y - scale * y) * olayer.extent / (double) scale);
		}

		if (mb_geometry[t] == VT_POLYGON) {
			dv = clean_or_clip_poly(dv, 0, 0, 0, false);
			if (dv.size() < 3) {
				dv.clear();
			}
		}
		dv = remove_noop(dv, mb_geometry[t], 0);
		if (mb_geometry[t] == VT_POLYGON) {
			dv = close_poly(dv);
		}

		if (dv.size() > 0) {
			mvt_feature feature;
			feature.type = mb_geometry[t];
			feature.geometry = to_feature(dv);

			json_object *id = json_hash_get(j, "id");
			if (id != NULL) {
				feature.id = atoll(id->string);
				feature.has_id = true;
			}

			for (size_t i = 0; i < properties->length; i++) {
				int tp = -1;
				std::string s;

				stringify_value(properties->values[i], tp, s, "Filter output", jp->line, j);
				if (tp >= 0) {
					mvt_value v = stringified_to_mvt_value(tp, s.c_str());
					ret.tag(feature, std::string(properties->keys[i]->string), v);
				}
			}

			ret.features.push_back(feature);
		}

		json_free(j);
	}

	json_end(jp);
	return ret;
}

static pthread_mutex_t pipe_lock = PTHREAD_MUTEX_INITIALIZER;

void setup_filter(const char *filter, int *write_to, int *read_from, pid_t *pid, unsigned z, unsigned x, unsigned y) {
	// This will create two pipes, a new thread, and a new process.
	//
	// The new process will read from one pipe and write to the other, and execute the filter.
	// The new thread will write the GeoJSON to the pipe that leads to the filter.
	// The original thread will read the GeoJSON from the filter and convert it back into vector tiles.

	if (pthread_mutex_lock(&pipe_lock) != 0) {
		perror("pthread_mutex_lock (pipe)");
		exit(EXIT_FAILURE);
	}

	int pipe_orig[2], pipe_filtered[2];
	if (pipe(pipe_orig) < 0) {
		perror("pipe (original features)");
		exit(EXIT_FAILURE);
	}
	if (pipe(pipe_filtered) < 0) {
		perror("pipe (filtered features)");
		exit(EXIT_FAILURE);
	}

	std::string z_str = std::to_string(z);
	std::string x_str = std::to_string(x);
	std::string y_str = std::to_string(y);

	*pid = fork();
	if (*pid < 0) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (*pid == 0) {
		// child

		if (dup2(pipe_orig[0], 0) < 0) {
			perror("dup child stdin");
			exit(EXIT_FAILURE);
		}
		if (dup2(pipe_filtered[1], 1) < 0) {
			perror("dup child stdout");
			exit(EXIT_FAILURE);
		}
		if (close(pipe_orig[1]) != 0) {
			perror("close output to filter");
			exit(EXIT_FAILURE);
		}
		if (close(pipe_filtered[0]) != 0) {
			perror("close input from filter");
			exit(EXIT_FAILURE);
		}
		if (close(pipe_orig[0]) != 0) {
			perror("close dup input of filter");
			exit(EXIT_FAILURE);
		}
		if (close(pipe_filtered[1]) != 0) {
			perror("close dup output of filter");
			exit(EXIT_FAILURE);
		}

		// XXX close other fds?

		if (execlp("sh", "sh", "-c", filter, "sh", z_str.c_str(), x_str.c_str(), y_str.c_str(), NULL) != 0) {
			perror("exec");
			exit(EXIT_FAILURE);
		}
	} else {
		// parent

		if (close(pipe_orig[0]) != 0) {
			perror("close filter-side reader");
			exit(EXIT_FAILURE);
		}
		if (close(pipe_filtered[1]) != 0) {
			perror("close filter-side writer");
			exit(EXIT_FAILURE);
		}
		if (fcntl(pipe_orig[1], F_SETFD, FD_CLOEXEC) != 0) {
			perror("cloxec output to filter");
			exit(EXIT_FAILURE);
		}
		if (fcntl(pipe_filtered[0], F_SETFD, FD_CLOEXEC) != 0) {
			perror("cloxec input from filter");
			exit(EXIT_FAILURE);
		}

		if (pthread_mutex_unlock(&pipe_lock) != 0) {
			perror("pthread_mutex_unlock (pipe_lock)");
			exit(EXIT_FAILURE);
		}

		*write_to = pipe_orig[1];
		*read_from = pipe_filtered[0];
	}
}

mvt_layer filter_layer(const char *filter, mvt_layer &layer, unsigned z, unsigned x, unsigned y) {
	int write_to, read_from;
	pid_t pid;
	setup_filter(filter, &write_to, &read_from, &pid, z, x, y);

	writer_arg wa;
	wa.write_to = write_to;
	wa.layer = &layer;
	wa.z = z;
	wa.x = x;
	wa.y = y;

	pthread_t writer;
	if (pthread_create(&writer, NULL, run_writer, &wa) != 0) {
		perror("pthread_create (filter writer)");
		exit(EXIT_FAILURE);
	}

	layer = parse_layer(read_from, z, x, y, layer);

	if (close(read_from) != 0) {
		perror("close output from filter");
		exit(EXIT_FAILURE);
	}

	while (1) {
		int stat_loc;
		if (waitpid(pid, &stat_loc, 0) < 0) {
			perror("waitpid for filter\n");
			exit(EXIT_FAILURE);
		}
		if (WIFEXITED(stat_loc) || WIFSIGNALED(stat_loc)) {
			break;
		}
	}

	void *ret;
	if (pthread_join(writer, &ret) != 0) {
		perror("pthread_join filter writer");
		exit(EXIT_FAILURE);
	}

	return layer;
}
