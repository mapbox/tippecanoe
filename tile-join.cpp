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
#include "mvt.hpp"
#include "projection.hpp"
#include "pool.hpp"
#include "mbtiles.hpp"
#include "geometry.hpp"

std::string dequote(std::string s);

struct stats {
	int minzoom;
	int maxzoom;
	double midlat, midlon;
	double minlat, minlon, maxlat, maxlon;
};

void handle(std::string message, int z, unsigned x, unsigned y, std::vector<std::set<type_and_string> > &file_keys, char ***layernames, int *nlayers, sqlite3 *outdb, std::vector<std::string> &header, std::map<std::string, std::vector<std::string> > &mapping, std::set<std::string> &exclude, int ifmatched) {
	mvt_tile tile;
	mvt_tile outtile;
	int features_added = 0;

	if (!tile.decode(message)) {
		fprintf(stderr, "Couldn't decompress tile %d/%u/%u\n", z, x, y);
		exit(EXIT_FAILURE);
	}

	for (size_t l = 0; l < tile.layers.size(); l++) {
		mvt_layer &layer = tile.layers[l];
		mvt_layer outlayer;

		outlayer.name = layer.name;
		outlayer.version = layer.version;
		outlayer.extent = layer.extent;

		const char *ln = layer.name.c_str();

		int ll;
		for (ll = 0; ll < *nlayers; ll++) {
			if (strcmp((*layernames)[ll], ln) == 0) {
				break;
			}
		}
		if (ll == *nlayers) {
			file_keys.push_back(std::set<type_and_string>());
			*layernames = (char **) realloc(*layernames, (ll + 1) * sizeof(char *));

			if (*layernames == NULL) {
				perror("realloc layernames");
				exit(EXIT_FAILURE);
			}

			(*layernames)[ll] = strdup(ln);
			if ((*layernames)[ll] == NULL) {
				perror("Out of memory");
				exit(EXIT_FAILURE);
			}
			*nlayers = ll + 1;
		}

		for (size_t f = 0; f < layer.features.size(); f++) {
			mvt_feature feat = layer.features[f];
			mvt_feature outfeature;
			int matched = 0;

			for (int t = 0; t + 1 < feat.tags.size(); t += 2) {
				const char *key = layer.keys[feat.tags[t]].c_str();
				mvt_value &val = layer.values[feat.tags[t + 1]];
				char *value;
				int type = -1;

				if (val.type == mvt_string) {
					value = strdup(val.string_value.c_str());
					if (value == NULL) {
						perror("Out of memory");
						exit(EXIT_FAILURE);
					}
					type = VT_STRING;
				} else if (val.type == mvt_int) {
					if (asprintf(&value, "%lld", (long long) val.numeric_value.int_value) >= 0) {
						type = VT_NUMBER;
					}
				} else if (val.type == mvt_double) {
					if (asprintf(&value, "%g", val.numeric_value.double_value) >= 0) {
						type = VT_NUMBER;
					}
				} else if (val.type == mvt_float) {
					if (asprintf(&value, "%g", val.numeric_value.float_value) >= 0) {
						type = VT_NUMBER;
					}
				} else if (val.type == mvt_bool) {
					if (asprintf(&value, "%s", val.numeric_value.bool_value ? "true" : "false") >= 0) {
						type = VT_BOOLEAN;
					}
				} else if (val.type == mvt_sint) {
					if (asprintf(&value, "%lld", (long long) val.numeric_value.sint_value) >= 0) {
						type = VT_NUMBER;
					}
				} else if (val.type == mvt_uint) {
					if (asprintf(&value, "%llu", (long long) val.numeric_value.uint_value) >= 0) {
						type = VT_NUMBER;
					}
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
					file_keys[ll].insert(tas);
					outlayer.tag(outfeature, layer.keys[feat.tags[t]], val);
				}

				if (header.size() > 0 && strcmp(key, header[0].c_str()) == 0) {
					std::map<std::string, std::vector<std::string> >::iterator ii = mapping.find(std::string(value));

					if (ii != mapping.end()) {
						std::vector<std::string> fields = ii->second;
						matched = 1;

						for (size_t i = 1; i < fields.size(); i++) {
							std::string joinkey = header[i];
							std::string joinval = fields[i];
							int type = VT_STRING;

							if (joinval.size() > 0) {
								if (joinval[0] == '"') {
									joinval = dequote(joinval);
								} else if ((joinval[0] >= '0' && joinval[0] <= '9') || joinval[0] == '-') {
									type = VT_NUMBER;
								}
							}

							const char *sjoinkey = joinkey.c_str();

							if (exclude.count(joinkey) == 0) {
								type_and_string tas;
								tas.string = std::string(sjoinkey);
								tas.type = type;
								file_keys[ll].insert(tas);
								outlayer.tag(outfeature, layer.keys[feat.tags[t]], val);

								mvt_value outval;
								if (type == VT_STRING) {
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

				free(value);
			}

			if (matched || !ifmatched) {
				outfeature.type = feat.type;
				outfeature.geometry = feat.geometry;

				features_added++;
				outlayer.features.push_back(outfeature);
			}
		}

		outtile.layers.push_back(outlayer);
	}

	if (features_added == 0) {
		return;
	}

	std::string compressed = outtile.encode();

	if (compressed.size() > 500000) {
		fprintf(stderr, "Tile %d/%u/%u size is %lld, >500000. Skipping this tile\n.", z, x, y, (long long) compressed.size());
		return;
	}

	mbtiles_write_tile(outdb, z, x, y, compressed.data(), compressed.size());
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

void decode(char *fname, char *map, std::vector<std::set<type_and_string> > &file_keys, char ***layernames, int *nlayers, sqlite3 *outdb, struct stats *st, std::vector<std::string> &header, std::map<std::string, std::vector<std::string> > &mapping, std::set<std::string> &exclude, int ifmatched, char **attribution) {
	sqlite3 *db;

	if (sqlite3_open(fname, &db) != SQLITE_OK) {
		fprintf(stderr, "%s: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	const char *sql = "SELECT zoom_level, tile_column, tile_row, tile_data from tiles;";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		long long zoom = sqlite3_column_int(stmt, 0);
		long long x = sqlite3_column_int(stmt, 1);
		long long y = sqlite3_column_int(stmt, 2);
		y = (1LL << zoom) - 1 - y;

		int len = sqlite3_column_bytes(stmt, 3);
		const char *s = (const char *) sqlite3_column_blob(stmt, 3);

		fprintf(stderr, "%lld/%lld/%lld   \r", zoom, x, y);

		handle(std::string(s, len), zoom, x, y, file_keys, layernames, nlayers, outdb, header, mapping, exclude, ifmatched);
	}

	sqlite3_finalize(stmt);

	if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'minzoom'", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			int minzoom = sqlite3_column_int(stmt, 0);
			st->minzoom = min(st->minzoom, minzoom);
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'maxzoom'", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			int maxzoom = sqlite3_column_int(stmt, 0);
			st->maxzoom = max(st->maxzoom, maxzoom);
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'center'", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			const unsigned char *s = sqlite3_column_text(stmt, 0);
			sscanf((char *) s, "%lf,%lf", &st->midlon, &st->midlat);
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'attribution'", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			*attribution = strdup((char *) sqlite3_column_text(stmt, 0));
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'bounds'", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			const unsigned char *s = sqlite3_column_text(stmt, 0);
			double minlon, minlat, maxlon, maxlat;
			sscanf((char *) s, "%lf,%lf,%lf,%lf", &minlon, &minlat, &maxlon, &maxlat);
			st->minlon = min(minlon, st->minlon);
			st->maxlon = max(maxlon, st->maxlon);
			st->minlat = min(minlat, st->minlat);
			st->maxlat = max(maxlat, st->maxlat);
		}
		sqlite3_finalize(stmt);
	}

	if (sqlite3_close(db) != SQLITE_OK) {
		fprintf(stderr, "%s: could not close database: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s [-f] [-i] [-c joins.csv] [-x exclude ...] -o new.mbtiles source.mbtiles ...\n", argv[0]);
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

void readcsv(char *fn, std::vector<std::string> &header, std::map<std::string, std::vector<std::string> > &mapping) {
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
			mapping.insert(std::pair<std::string, std::vector<std::string> >(line[0], line));
		}
	}

	fclose(f);
}

int main(int argc, char **argv) {
	char *outfile = NULL;
	char *csv = NULL;
	int force = 0;
	int ifmatched = 0;

	std::vector<std::string> header;
	std::map<std::string, std::vector<std::string> > mapping;

	std::set<std::string> exclude;

	extern int optind;
	extern char *optarg;
	int i;

	while ((i = getopt(argc, argv, "fo:c:x:i")) != -1) {
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

	std::vector<std::set<type_and_string> > file_keys;
	char **layernames = NULL;
	int nlayers = 0;
	char *attribution = NULL;

	for (i = optind; i < argc; i++) {
		decode(argv[i], csv, file_keys, &layernames, &nlayers, outdb, &st, header, mapping, exclude, ifmatched, &attribution);
	}

	mbtiles_write_metadata(outdb, outfile, layernames, st.minzoom, st.maxzoom, st.minlat, st.minlon, st.maxlat, st.maxlon, st.midlat, st.midlon, file_keys, nlayers, 0, attribution);
	mbtiles_close(outdb, argv);

	return 0;
}
