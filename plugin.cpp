#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <map>
#include <pthread.h>
#include <unistd.h>
#include "mvt.hpp"
#include "plugin.hpp"
#include "projection.hpp"
#include "geometry.hpp"

struct writer_arg {
	int *pipe_orig;
	mvt_layer *layer;
	unsigned z;
	unsigned x;
	unsigned y;
};

void *run_writer(void *a) {
	writer_arg *wa = (writer_arg *) a;

	// XXX worry about SIGPIPE?

	FILE *fp = fdopen(wa->pipe_orig[1], "w");
	if (fp == NULL) {
		perror("fdopen (pipe writer)");
		exit(EXIT_FAILURE);
	}

	layer_to_geojson(fp, *(wa->layer), wa->z, wa->x, wa->y);

	if (fclose(fp) != 0) {
		perror("fclose output to filter");
		exit(EXIT_FAILURE);
	}

	return NULL;
}

mvt_layer filter_layer(const char *filter, mvt_layer &layer, unsigned z, unsigned x, unsigned y) {
	// This will create two pipes, a new thread, and a new process.
	//
	// The new process will read from one pipe and write to the other, and execute the filter.
	// The new thread will write the GeoJSON to the pipe that leads to the filter.
	// The original thread will read the GeoJSON from the filter and convert it back into vector tiles.

	int pipe_orig[2], pipe_filtered[2];
	if (pipe(pipe_orig) < 0) {
		perror("pipe (original features)");
		exit(EXIT_FAILURE);
	}
	if (pipe(pipe_filtered) < 0) {
		perror("pipe (filtered features)");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (pid == 0) {
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

		// XXX close other fds?

		// XXX add zyx args
		if (execlp("sh", "sh", "-c", filter, NULL) != 0) {
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

		writer_arg wa;
		wa.pipe_orig = pipe_orig;
		wa.layer = &layer;
		wa.z = z;
		wa.x = x;
		wa.y = y;

		pthread_t writer;
		if (pthread_create(&writer, NULL, run_writer, &wa) != 0) {
			perror("pthread_create (filter writer)");
			exit(EXIT_FAILURE);
		}

		char buf[200];
		size_t count;
		while ((count = read(pipe_filtered[0], buf, 200)) != 0) {
			write(1, buf, count);
		}

		int stat_loc;
		if (waitpid(pid, &stat_loc, 0) < 0) {
			perror("waitpid for filter\n");
			exit(EXIT_FAILURE);
		}

		if (close(pipe_filtered[0]) != 0) {
			perror("close output from filter");
			exit(EXIT_FAILURE);
		}

		void *ret;
		if (pthread_join(writer, &ret) != 0) {
			perror("pthread_join filter writer");
			exit(EXIT_FAILURE);
		}
	}

	return layer;
}

struct lonlat {
	int op;
	double lon;
	double lat;
	int x;
	int y;

	lonlat(int nop, double nlon, double nlat, int nx, int ny) {
		this->op = nop;
		this->lon = nlon;
		this->lat = nlat;
		this->x = nx;
		this->y = ny;
	}
};

void layer_to_geojson(FILE *fp, mvt_layer &layer, unsigned z, unsigned x, unsigned y) {
	for (size_t f = 0; f < layer.features.size(); f++) {
		mvt_feature &feat = layer.features[f];

		if (f != 0) {
			fprintf(fp, ",\n");
		}

		fprintf(fp, "{ \"type\": \"Feature\"");

		if (feat.has_id) {
			fprintf(fp, ", \"id\": %llu", feat.id);
		}

		fprintf(fp, ", \"properties\": { ");

		for (size_t t = 0; t + 1 < feat.tags.size(); t += 2) {
			if (t != 0) {
				fprintf(fp, ", ");
			}

			if (feat.tags[t] >= layer.keys.size()) {
				fprintf(stderr, "Error: out of bounds feature key\n");
				exit(EXIT_FAILURE);
			}
			if (feat.tags[t + 1] >= layer.values.size()) {
				fprintf(stderr, "Error: out of bounds feature value\n");
				exit(EXIT_FAILURE);
			}

			const char *key = layer.keys[feat.tags[t]].c_str();
			mvt_value const &val = layer.values[feat.tags[t + 1]];

			if (val.type == mvt_string) {
				fprintq(fp, key);
				fprintf(fp, ": ");
				fprintq(fp, val.string_value.c_str());
			} else if (val.type == mvt_int) {
				fprintq(fp, key);
				fprintf(fp, ": %lld", (long long) val.numeric_value.int_value);
			} else if (val.type == mvt_double) {
				fprintq(fp, key);
				double v = val.numeric_value.double_value;
				if (v == (long long) v) {
					fprintf(fp, ": %lld", (long long) v);
				} else {
					fprintf(fp, ": %g", v);
				}
			} else if (val.type == mvt_float) {
				fprintq(fp, key);
				double v = val.numeric_value.float_value;
				if (v == (long long) v) {
					fprintf(fp, ": %lld", (long long) v);
				} else {
					fprintf(fp, ": %g", v);
				}
			} else if (val.type == mvt_sint) {
				fprintq(fp, key);
				fprintf(fp, ": %lld", (long long) val.numeric_value.sint_value);
			} else if (val.type == mvt_uint) {
				fprintq(fp, key);
				fprintf(fp, ": %lld", (long long) val.numeric_value.uint_value);
			} else if (val.type == mvt_bool) {
				fprintq(fp, key);
				fprintf(fp, ": %s", val.numeric_value.bool_value ? "true" : "false");
			}
		}

		fprintf(fp, " }, \"geometry\": { ");

		std::vector<lonlat> ops;

		for (size_t g = 0; g < feat.geometry.size(); g++) {
			int op = feat.geometry[g].op;
			long long px = feat.geometry[g].x;
			long long py = feat.geometry[g].y;

			if (op == VT_MOVETO || op == VT_LINETO) {
				long long scale = 1LL << (32 - z);
				long long wx = scale * x + (scale / layer.extent) * px;
				long long wy = scale * y + (scale / layer.extent) * py;

				double lat, lon;
				projection->unproject(wx, wy, 32, &lon, &lat);

				ops.push_back(lonlat(op, lon, lat, px, py));
			} else {
				ops.push_back(lonlat(op, 0, 0, 0, 0));
			}
		}

		if (feat.type == VT_POINT) {
			if (ops.size() == 1) {
				fprintf(fp, "\"type\": \"Point\", \"coordinates\": [ %f, %f ]", ops[0].lon, ops[0].lat);
			} else {
				fprintf(fp, "\"type\": \"MultiPoint\", \"coordinates\": [ ");
				for (size_t i = 0; i < ops.size(); i++) {
					if (i != 0) {
						fprintf(fp, ", ");
					}
					fprintf(fp, "[ %f, %f ]", ops[i].lon, ops[i].lat);
				}
				fprintf(fp, " ]");
			}
		} else if (feat.type == VT_LINE) {
			int movetos = 0;
			for (size_t i = 0; i < ops.size(); i++) {
				if (ops[i].op == VT_MOVETO) {
					movetos++;
				}
			}

			if (movetos < 2) {
				fprintf(fp, "\"type\": \"LineString\", \"coordinates\": [ ");
				for (size_t i = 0; i < ops.size(); i++) {
					if (i != 0) {
						fprintf(fp, ", ");
					}
					fprintf(fp, "[ %f, %f ]", ops[i].lon, ops[i].lat);
				}
				fprintf(fp, " ]");
			} else {
				fprintf(fp, "\"type\": \"MultiLineString\", \"coordinates\": [ [ ");
				int state = 0;
				for (size_t i = 0; i < ops.size(); i++) {
					if (ops[i].op == VT_MOVETO) {
						if (state == 0) {
							fprintf(fp, "[ %f, %f ]", ops[i].lon, ops[i].lat);
							state = 1;
						} else {
							fprintf(fp, " ], [ ");
							fprintf(fp, "[ %f, %f ]", ops[i].lon, ops[i].lat);
							state = 1;
						}
					} else {
						fprintf(fp, ", [ %f, %f ]", ops[i].lon, ops[i].lat);
					}
				}
				fprintf(fp, " ] ]");
			}
		} else if (feat.type == VT_POLYGON) {
			std::vector<std::vector<lonlat> > rings;
			std::vector<double> areas;

			for (size_t i = 0; i < ops.size(); i++) {
				if (ops[i].op == VT_MOVETO) {
					rings.push_back(std::vector<lonlat>());
					areas.push_back(0);
				}

				int n = rings.size() - 1;
				if (n >= 0) {
					if (ops[i].op == VT_CLOSEPATH) {
						rings[n].push_back(rings[n][0]);
					} else {
						rings[n].push_back(ops[i]);
					}
				}
			}

			int outer = 0;

			for (size_t i = 0; i < rings.size(); i++) {
				long double area = 0;
				for (size_t k = 0; k < rings[i].size(); k++) {
					if (rings[i][k].op != VT_CLOSEPATH) {
						area += rings[i][k].x * rings[i][(k + 1) % rings[i].size()].y;
						area -= rings[i][k].y * rings[i][(k + 1) % rings[i].size()].x;
					}
				}

				areas[i] = area;
				if (areas[i] >= 0 || i == 0) {
					outer++;
				}

				// fprintf(fp, "area %f\n", area / .00000274 / .00000274);
			}

			if (outer > 1) {
				fprintf(fp, "\"type\": \"MultiPolygon\", \"coordinates\": [ [ [ ");
			} else {
				fprintf(fp, "\"type\": \"Polygon\", \"coordinates\": [ [ ");
			}

			int state = 0;
			for (size_t i = 0; i < rings.size(); i++) {
				if (areas[i] >= 0) {
					if (state != 0) {
						// new multipolygon
						fprintf(fp, " ] ], [ [ ");
					}
					state = 1;
				}

				if (state == 2) {
					// new ring in the same polygon
					fprintf(fp, " ], [ ");
				}

				for (size_t j = 0; j < rings[i].size(); j++) {
					if (rings[i][j].op != VT_CLOSEPATH) {
						if (j != 0) {
							fprintf(fp, ", ");
						}

						fprintf(fp, "[ %f, %f ]", rings[i][j].lon, rings[i][j].lat);
					} else {
						if (j != 0) {
							fprintf(fp, ", ");
						}

						fprintf(fp, "[ %f, %f ]", rings[i][0].lon, rings[i][0].lat);
					}
				}

				state = 2;
			}

			if (outer > 1) {
				fprintf(fp, " ] ] ]");
			} else {
				fprintf(fp, " ] ]");
			}
		}

		fprintf(fp, " } }\n");
	}
}

void fprintq(FILE *fp, const char *s) {
	fputc('"', fp);
	for (; *s; s++) {
		if (*s == '\\' || *s == '"') {
			fprintf(fp, "\\%c", *s);
		} else if (*s >= 0 && *s < ' ') {
			fprintf(fp, "\\u%04x", *s);
		} else {
			fputc(*s, fp);
		}
	}
	fputc('"', fp);
}
