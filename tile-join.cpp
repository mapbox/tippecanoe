#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <limits.h>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <zlib.h>
#include <math.h>
#include <pthread.h>
#include "mvt.hpp"
#include "projection.hpp"
#include "pool.hpp"
#include "mbtiles.hpp"
#include "geometry.hpp"

std::string dequote(std::string s);

bool pk = false;
size_t CPUS;

struct stats {
	int minzoom;
	int maxzoom;
	double midlat, midlon;
	double minlat, minlon, maxlat, maxlon;
};

void handle(std::string message, int z, unsigned x, unsigned y, std::map<std::string, layermap_entry> &layermap, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping, std::set<std::string> &exclude, int ifmatched, mvt_tile &outtile) {
	mvt_tile tile;
	int features_added = 0;

	if (!tile.decode(message)) {
		fprintf(stderr, "Couldn't decompress tile %d/%u/%u\n", z, x, y);
		exit(EXIT_FAILURE);
	}

	for (size_t l = 0; l < tile.layers.size(); l++) {
		mvt_layer &layer = tile.layers[l];

		size_t ol;
		for (ol = 0; ol < outtile.layers.size(); ol++) {
			if (tile.layers[l].name == outtile.layers[ol].name) {
				break;
			}
		}

		if (ol == outtile.layers.size()) {
			outtile.layers.push_back(mvt_layer());

			outtile.layers[ol].name = layer.name;
			outtile.layers[ol].version = layer.version;
			outtile.layers[ol].extent = layer.extent;
		}

		mvt_layer &outlayer = outtile.layers[ol];

		if (layer.extent != outlayer.extent) {
			if (layer.extent > outlayer.extent) {
				for (size_t i = 0; i < outlayer.features.size(); i++) {
					for (size_t j = 0; j < outlayer.features[i].geometry.size(); j++) {
						outlayer.features[i].geometry[j].x = outlayer.features[i].geometry[j].x * layer.extent / outlayer.extent;
						outlayer.features[i].geometry[j].y = outlayer.features[i].geometry[j].y * layer.extent / outlayer.extent;
					}
				}

				outlayer.extent = layer.extent;
			}
		}

		if (layermap.count(layer.name) == 0) {
			layermap.insert(std::pair<std::string, layermap_entry>(layer.name, layermap_entry(layermap.size())));
			auto file_keys = layermap.find(layer.name);
			file_keys->second.minzoom = z;
			file_keys->second.maxzoom = z;
		}
		auto file_keys = layermap.find(layer.name);

		for (size_t f = 0; f < layer.features.size(); f++) {
			mvt_feature feat = layer.features[f];
			mvt_feature outfeature;
			int matched = 0;

			if (feat.has_id) {
				outfeature.has_id = true;
				outfeature.id = feat.id;
			}

			for (size_t t = 0; t + 1 < feat.tags.size(); t += 2) {
				const char *key = layer.keys[feat.tags[t]].c_str();
				mvt_value &val = layer.values[feat.tags[t + 1]];
				std::string value;
				int type = -1;

				if (val.type == mvt_string) {
					value = val.string_value;
					type = VT_STRING;
				} else if (val.type == mvt_int) {
					aprintf(&value, "%lld", (long long) val.numeric_value.int_value);
					type = VT_NUMBER;
				} else if (val.type == mvt_double) {
					aprintf(&value, "%g", val.numeric_value.double_value);
					type = VT_NUMBER;
				} else if (val.type == mvt_float) {
					aprintf(&value, "%g", val.numeric_value.float_value);
					type = VT_NUMBER;
				} else if (val.type == mvt_bool) {
					aprintf(&value, "%s", val.numeric_value.bool_value ? "true" : "false");
					type = VT_BOOLEAN;
				} else if (val.type == mvt_sint) {
					aprintf(&value, "%lld", (long long) val.numeric_value.sint_value);
					type = VT_NUMBER;
				} else if (val.type == mvt_uint) {
					aprintf(&value, "%llu", (long long) val.numeric_value.uint_value);
					type = VT_NUMBER;
				} else {
					continue;
				}

				if (type < 0) {
					continue;
				}

				if (exclude.count(std::string(key)) == 0) {
					type_and_string tas;
					tas.string = std::string(key);
					tas.type = type;
					file_keys->second.file_keys.insert(tas);
					outlayer.tag(outfeature, layer.keys[feat.tags[t]], val);
				}

				if (header.size() > 0 && strcmp(key, header[0].c_str()) == 0) {
					std::map<std::string, std::vector<std::string>>::iterator ii = mapping.find(value);

					if (ii != mapping.end()) {
						std::vector<std::string> fields = ii->second;
						matched = 1;

						for (size_t i = 1; i < fields.size(); i++) {
							std::string joinkey = header[i];
							std::string joinval = fields[i];
							int attr_type = VT_STRING;

							if (joinval.size() > 0) {
								if (joinval[0] == '"') {
									joinval = dequote(joinval);
								} else if ((joinval[0] >= '0' && joinval[0] <= '9') || joinval[0] == '-') {
									attr_type = VT_NUMBER;
								}
							}

							const char *sjoinkey = joinkey.c_str();

							if (exclude.count(joinkey) == 0) {
								type_and_string tas;
								tas.string = std::string(sjoinkey);
								tas.type = attr_type;
								file_keys->second.file_keys.insert(tas);

								mvt_value outval;
								if (attr_type == VT_STRING) {
									outval.type = mvt_string;
									outval.string_value = joinval;
								} else {
									outval.type = mvt_double;
									outval.numeric_value.double_value = atof(joinval.c_str());
								}

								outlayer.tag(outfeature, joinkey, outval);
							}
						}
					}
				}
			}

			if (matched || !ifmatched) {
				outfeature.type = feat.type;
				outfeature.geometry = feat.geometry;

				if (layer.extent != outlayer.extent) {
					for (size_t i = 0; i < outfeature.geometry.size(); i++) {
						outfeature.geometry[i].x = outfeature.geometry[i].x * outlayer.extent / layer.extent;
						outfeature.geometry[i].y = outfeature.geometry[i].y * outlayer.extent / layer.extent;
					}
				}

				features_added++;
				outlayer.features.push_back(outfeature);

				if (z < file_keys->second.minzoom) {
					file_keys->second.minzoom = z;
				}
				if (z > file_keys->second.maxzoom) {
					file_keys->second.maxzoom = z;
				}
			}
		}
	}

	if (features_added == 0) {
		return;
	}
}

double min(double a, double b) {
	if (a < b) {
		return a;
	} else {
		return b;
	}
}

double max(double a, double b) {
	if (a > b) {
		return a;
	} else {
		return b;
	}
}

struct reader {
	long long zoom;
	long long x;
	long long sorty;
	long long y;

	std::string data;

	sqlite3 *db;
	sqlite3_stmt *stmt;
	struct reader *next;

	bool operator<(const struct reader &r) const {
		if (zoom < r.zoom) {
			return true;
		}
		if (zoom > r.zoom) {
			return false;
		}

		if (x < r.x) {
			return true;
		}
		if (x > r.x) {
			return false;
		}

		if (sorty < r.sorty) {
			return true;
		}
		if (sorty > r.sorty) {
			return false;
		}

		if (data < r.data) {
			return true;
		}

		return false;
	}
};

struct reader *begin_reading(char *fname) {
	sqlite3 *db;

	if (sqlite3_open(fname, &db) != SQLITE_OK) {
		fprintf(stderr, "%s: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	const char *sql = "SELECT zoom_level, tile_column, tile_row, tile_data from tiles order by zoom_level, tile_column, tile_row;";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	struct reader *r = new reader;
	r->db = db;
	r->stmt = stmt;
	r->next = NULL;

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		r->zoom = sqlite3_column_int(stmt, 0);
		r->x = sqlite3_column_int(stmt, 1);
		r->sorty = sqlite3_column_int(stmt, 2);
		r->y = (1LL << r->zoom) - 1 - r->sorty;

		const char *data = (const char *) sqlite3_column_blob(stmt, 3);
		size_t len = sqlite3_column_bytes(stmt, 3);

		r->data = std::string(data, len);
	} else {
		r->zoom = 32;
	}

	return r;
}

struct zxy {
	long long z;
	long long x;
	long long y;

	zxy(long long _z, long long _x, long long _y) {
		z = _z;
		x = _x;
		y = _y;
	}

	bool operator<(zxy const &other) const {
		if (z < other.z) {
			return true;
		}
		if (z > other.z) {
			return false;
		}

		if (x < other.x) {
			return true;
		}
		if (x > other.x) {
			return false;
		}

		if (y < other.y) {
			return true;
		}

		return false;
	}
};

struct arg {
	std::map<zxy, std::vector<std::string>> inputs;
	std::map<zxy, std::string> outputs;

	std::map<std::string, layermap_entry> *layermap;

	std::vector<std::string> *header;
	std::map<std::string, std::vector<std::string>> *mapping;
	std::set<std::string> *exclude;
	int ifmatched;
};

void *join_worker(void *v) {
	arg *a = (arg *) v;

	for (auto ai = a->inputs.begin(); ai != a->inputs.end(); ++ai) {
		mvt_tile tile;

		for (size_t i = 0; i < ai->second.size(); i++) {
			handle(ai->second[i], ai->first.z, ai->first.x, ai->first.y, *(a->layermap), *(a->header), *(a->mapping), *(a->exclude), a->ifmatched, tile);
		}

		ai->second.clear();

		bool anything = false;
		for (size_t i = 0; i < tile.layers.size(); i++) {
			if (tile.layers[i].features.size() > 0) {
				anything = true;
				break;
			}
		}

		if (anything) {
			std::string compressed = tile.encode();

			if (!pk && compressed.size() > 500000) {
				fprintf(stderr, "Tile %lld/%lld/%lld size is %lld, >500000. Skipping this tile\n.", ai->first.z, ai->first.x, ai->first.y, (long long) compressed.size());
			} else {
				a->outputs.insert(std::pair<zxy, std::string>(ai->first, compressed));
			}
		}
	}

	return NULL;
}

void handle_tasks(std::map<zxy, std::vector<std::string>> &tasks, std::vector<std::map<std::string, layermap_entry>> &layermaps, sqlite3 *outdb, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping, std::set<std::string> &exclude, int ifmatched) {
	pthread_t pthreads[CPUS];
	std::vector<arg> args;

	for (size_t i = 0; i < CPUS; i++) {
		args.push_back(arg());

		args[i].layermap = &layermaps[i];
		args[i].header = &header;
		args[i].mapping = &mapping;
		args[i].exclude = &exclude;
		args[i].ifmatched = ifmatched;
	}

	size_t count = 0;
	// This isn't careful about distributing tasks evenly across CPUs,
	// but, from testing, it actually takes a little longer to do
	// the proper allocation than is saved by perfectly balanced threads.
	for (auto ai = tasks.begin(); ai != tasks.end(); ++ai) {
		args[count].inputs.insert(*ai);
		count = (count + 1) % CPUS;

		if (ai == tasks.begin()) {
			fprintf(stderr, "%lld/%lld/%lld  \r", ai->first.z, ai->first.x, ai->first.y);
		}
	}

	for (size_t i = 0; i < CPUS; i++) {
		if (pthread_create(&pthreads[i], NULL, join_worker, &args[i]) != 0) {
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	for (size_t i = 0; i < CPUS; i++) {
		void *retval;

		if (pthread_join(pthreads[i], &retval) != 0) {
			perror("pthread_join");
		}

		for (auto ai = args[i].outputs.begin(); ai != args[i].outputs.end(); ++ai) {
			mbtiles_write_tile(outdb, ai->first.z, ai->first.x, ai->first.y, ai->second.data(), ai->second.size());
		}
	}
}

void decode(struct reader *readers, char *map, std::map<std::string, layermap_entry> &layermap, sqlite3 *outdb, struct stats *st, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping, std::set<std::string> &exclude, int ifmatched, std::string &attribution) {
	std::vector<std::map<std::string, layermap_entry>> layermaps;
	for (size_t i = 0; i < CPUS; i++) {
		layermaps.push_back(std::map<std::string, layermap_entry>());
	}

	std::map<zxy, std::vector<std::string>> tasks;

	while (readers != NULL && readers->zoom < 32) {
		reader *r = readers;
		readers = readers->next;
		r->next = NULL;

		zxy tile = zxy(r->zoom, r->x, r->y);
		if (tasks.count(tile) == 0) {
			tasks.insert(std::pair<zxy, std::vector<std::string>>(tile, std::vector<std::string>()));
		}
		auto f = tasks.find(tile);
		f->second.push_back(r->data);

		if (readers == NULL || readers->zoom != r->zoom || readers->x != r->x || readers->y != r->y) {
			if (tasks.size() > 100 * CPUS) {
				handle_tasks(tasks, layermaps, outdb, header, mapping, exclude, ifmatched);
				tasks.clear();
			}
		}

		if (sqlite3_step(r->stmt) == SQLITE_ROW) {
			r->zoom = sqlite3_column_int(r->stmt, 0);
			r->x = sqlite3_column_int(r->stmt, 1);
			r->sorty = sqlite3_column_int(r->stmt, 2);
			r->y = (1LL << r->zoom) - 1 - r->sorty;

			const char *data = (const char *) sqlite3_column_blob(r->stmt, 3);
			size_t len = sqlite3_column_bytes(r->stmt, 3);

			r->data = std::string(data, len);
		} else {
			r->zoom = 32;
		}

		struct reader **rr;

		for (rr = &readers; *rr != NULL; rr = &((*rr)->next)) {
			if (*r < **rr) {
				break;
			}
		}

		r->next = *rr;
		*rr = r;
	}

	handle_tasks(tasks, layermaps, outdb, header, mapping, exclude, ifmatched);
	layermap = merge_layermaps(layermaps);

	struct reader *next;
	for (struct reader *r = readers; r != NULL; r = next) {
		next = r->next;
		sqlite3_finalize(r->stmt);

		if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'minzoom'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				int minzoom = sqlite3_column_int(r->stmt, 0);
				st->minzoom = min(st->minzoom, minzoom);
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'maxzoom'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				int maxzoom = sqlite3_column_int(r->stmt, 0);
				st->maxzoom = max(st->maxzoom, maxzoom);
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'center'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				const unsigned char *s = sqlite3_column_text(r->stmt, 0);
				sscanf((char *) s, "%lf,%lf", &st->midlon, &st->midlat);
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'attribution'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				attribution = std::string((char *) sqlite3_column_text(r->stmt, 0));
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'bounds'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				const unsigned char *s = sqlite3_column_text(r->stmt, 0);
				double minlon, minlat, maxlon, maxlat;
				sscanf((char *) s, "%lf,%lf,%lf,%lf", &minlon, &minlat, &maxlon, &maxlat);
				st->minlon = min(minlon, st->minlon);
				st->maxlon = max(maxlon, st->maxlon);
				st->minlat = min(minlat, st->minlat);
				st->maxlat = max(maxlat, st->maxlat);
			}
			sqlite3_finalize(r->stmt);
		}

		if (sqlite3_close(r->db) != SQLITE_OK) {
			fprintf(stderr, "Could not close database: %s\n", sqlite3_errmsg(r->db));
			exit(EXIT_FAILURE);
		}

		delete r;
	}
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s [-f] [-i] [-pk] [-c joins.csv] [-x exclude ...] -o new.mbtiles source.mbtiles ...\n", argv[0]);
	exit(EXIT_FAILURE);
}

#define MAXLINE 10000 /* XXX */

std::vector<std::string> split(char *s) {
	std::vector<std::string> ret;

	while (*s && *s != '\n') {
		char *start = s;
		int within = 0;

		for (; *s && *s != '\n'; s++) {
			if (*s == '"') {
				within = !within;
			}

			if (*s == ',' && !within) {
				break;
			}
		}

		std::string v = std::string(start, s - start);
		ret.push_back(v);

		if (*s == ',') {
			s++;

			while (*s && isspace(*s)) {
				s++;
			}
		}
	}

	return ret;
}

std::string dequote(std::string s) {
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '"') {
			if (i + 1 < s.size() && s[i + 1] == '"') {
				out.push_back('"');
			}
		} else {
			out.push_back(s[i]);
		}
	}
	return out;
}

void readcsv(char *fn, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping) {
	FILE *f = fopen(fn, "r");
	if (f == NULL) {
		perror(fn);
		exit(EXIT_FAILURE);
	}

	char s[MAXLINE];
	if (fgets(s, MAXLINE, f)) {
		header = split(s);

		for (size_t i = 0; i < header.size(); i++) {
			header[i] = dequote(header[i]);
		}
	}
	while (fgets(s, MAXLINE, f)) {
		std::vector<std::string> line = split(s);
		if (line.size() > 0) {
			line[0] = dequote(line[0]);
		}

		for (size_t i = 0; i < line.size() && i < header.size(); i++) {
			// printf("putting %s\n", line[0].c_str());
			mapping.insert(std::pair<std::string, std::vector<std::string>>(line[0], line));
		}
	}

	fclose(f);
}

int main(int argc, char **argv) {
	char *outfile = NULL;
	char *csv = NULL;
	int force = 0;
	int ifmatched = 0;

	CPUS = sysconf(_SC_NPROCESSORS_ONLN);
	if (CPUS < 1) {
		CPUS = 1;
	}

	std::vector<std::string> header;
	std::map<std::string, std::vector<std::string>> mapping;

	std::set<std::string> exclude;

	extern int optind;
	extern char *optarg;
	int i;

	while ((i = getopt(argc, argv, "fo:c:x:ip:")) != -1) {
		switch (i) {
		case 'o':
			outfile = optarg;
			break;

		case 'f':
			force = 1;
			break;

		case 'i':
			ifmatched = 1;
			break;

		case 'p':
			if (strcmp(optarg, "k") == 0) {
				pk = true;
			} else {
				fprintf(stderr, "%s: Unknown option for -p%s\n", argv[0], optarg);
				exit(EXIT_FAILURE);
			}
			break;

		case 'c':
			if (csv != NULL) {
				fprintf(stderr, "Only one -c for now\n");
				exit(EXIT_FAILURE);
			}

			csv = optarg;
			readcsv(csv, header, mapping);
			break;

		case 'x':
			exclude.insert(std::string(optarg));
			break;

		default:
			usage(argv);
		}
	}

	if (argc - optind < 1 || outfile == NULL) {
		usage(argv);
	}

	if (force) {
		unlink(outfile);
	}

	sqlite3 *outdb = mbtiles_open(outfile, argv, 0);
	struct stats st;
	memset(&st, 0, sizeof(st));
	st.minzoom = st.minlat = st.minlon = INT_MAX;
	st.maxzoom = st.maxlat = st.maxlon = INT_MIN;

	std::map<std::string, layermap_entry> layermap;
	std::string attribution;

	struct reader *readers = NULL;

	for (i = optind; i < argc; i++) {
		reader *r = begin_reading(argv[i]);
		struct reader **rr;

		for (rr = &readers; *rr != NULL; rr = &((*rr)->next)) {
			if (*r < **rr) {
				break;
			}
		}

		r->next = *rr;
		*rr = r;
	}

	decode(readers, csv, layermap, outdb, &st, header, mapping, exclude, ifmatched, attribution);

	mbtiles_write_metadata(outdb, outfile, st.minzoom, st.maxzoom, st.minlat, st.minlon, st.maxlat, st.maxlon, st.midlat, st.midlon, 0, attribution.size() != 0 ? attribution.c_str() : NULL, layermap);
	mbtiles_close(outdb, argv);

	return 0;
}
