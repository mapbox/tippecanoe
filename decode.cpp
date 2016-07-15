#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include <zlib.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <protozero/pbf_reader.hpp>
#include "mvt.hpp"
#include "projection.hpp"
#include "geometry.hpp"

void printq(const char *s) {
	putchar('"');
	for (; *s; s++) {
		if (*s == '\\' || *s == '"') {
			printf("\\%c", *s);
		} else if (*s >= 0 && *s < ' ') {
			printf("\\u%04x", *s);
		} else {
			putchar(*s);
		}
	}
	putchar('"');
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

void handle(std::string message, int z, unsigned x, unsigned y, int describe) {
	int within = 0;
	mvt_tile tile;

	try {
		if (!tile.decode(message)) {
			fprintf(stderr, "Couldn't parse tile %d/%u/%u\n", z, x, y);
			exit(EXIT_FAILURE);
		}
	} catch (protozero::unknown_pbf_wire_type_exception e) {
		fprintf(stderr, "PBF decoding error in tile %d/%u/%u\n", z, x, y);
		exit(EXIT_FAILURE);
	}

	printf("{ \"type\": \"FeatureCollection\"");

	if (describe) {
		printf(", \"properties\": { \"zoom\": %d, \"x\": %d, \"y\": %d }", z, x, y);

		if (projection != projections) {
			printf(", \"crs\": { \"type\": \"name\", \"properties\": { \"name\": ");
			printq(projection->alias);
			printf(" } }");
		}
	}

	printf(", \"features\": [\n");

	for (size_t l = 0; l < tile.layers.size(); l++) {
		mvt_layer &layer = tile.layers[l];
		int extent = layer.extent;

		if (describe) {
			if (l != 0) {
				printf(",\n");
			}

			printf("{ \"type\": \"FeatureCollection\"");
			printf(", \"properties\": { \"layer\": ");
			printq(layer.name.c_str());
			printf(", \"version\": %d, \"extent\": %d", layer.version, layer.extent);
			printf(" }");
			printf(", \"features\": [\n");

			within = 0;
		}

		for (size_t f = 0; f < layer.features.size(); f++) {
			mvt_feature &feat = layer.features[f];

			if (within) {
				printf(",\n");
			}
			within = 1;

			printf("{ \"type\": \"Feature\"");

			if (feat.has_id) {
				printf(", \"id\": %llu", feat.id);
			}

			printf(", \"properties\": { ");

			for (size_t t = 0; t + 1 < feat.tags.size(); t += 2) {
				if (t != 0) {
					printf(", ");
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
					printq(key);
					printf(": ");
					printq(val.string_value.c_str());
				} else if (val.type == mvt_int) {
					printq(key);
					printf(": %lld", (long long) val.numeric_value.int_value);
				} else if (val.type == mvt_double) {
					printq(key);
					double v = val.numeric_value.double_value;
					if (v == (long long) v) {
						printf(": %lld", (long long) v);
					} else {
						printf(": %g", v);
					}
				} else if (val.type == mvt_float) {
					printq(key);
					double v = val.numeric_value.float_value;
					if (v == (long long) v) {
						printf(": %lld", (long long) v);
					} else {
						printf(": %g", v);
					}
				} else if (val.type == mvt_sint) {
					printq(key);
					printf(": %lld", (long long) val.numeric_value.sint_value);
				} else if (val.type == mvt_uint) {
					printq(key);
					printf(": %lld", (long long) val.numeric_value.uint_value);
				} else if (val.type == mvt_bool) {
					printq(key);
					printf(": %s", val.numeric_value.bool_value ? "true" : "false");
				}
			}

			printf(" }, \"geometry\": { ");

			std::vector<lonlat> ops;

			for (size_t g = 0; g < feat.geometry.size(); g++) {
				int op = feat.geometry[g].op;
				long long px = feat.geometry[g].x;
				long long py = feat.geometry[g].y;

				if (op == VT_MOVETO || op == VT_LINETO) {
					long long scale = 1LL << (32 - z);
					long long wx = scale * x + (scale / extent) * px;
					long long wy = scale * y + (scale / extent) * py;

					double lat, lon;
					projection->unproject(wx, wy, 32, &lon, &lat);

					ops.push_back(lonlat(op, lon, lat, px, py));
				} else {
					ops.push_back(lonlat(op, 0, 0, 0, 0));
				}
			}

			if (feat.type == VT_POINT) {
				if (ops.size() == 1) {
					printf("\"type\": \"Point\", \"coordinates\": [ %f, %f ]", ops[0].lon, ops[0].lat);
				} else {
					printf("\"type\": \"MultiPoint\", \"coordinates\": [ ");
					for (size_t i = 0; i < ops.size(); i++) {
						if (i != 0) {
							printf(", ");
						}
						printf("[ %f, %f ]", ops[i].lon, ops[i].lat);
					}
					printf(" ]");
				}
			} else if (feat.type == VT_LINE) {
				int movetos = 0;
				for (size_t i = 0; i < ops.size(); i++) {
					if (ops[i].op == VT_MOVETO) {
						movetos++;
					}
				}

				if (movetos < 2) {
					printf("\"type\": \"LineString\", \"coordinates\": [ ");
					for (size_t i = 0; i < ops.size(); i++) {
						if (i != 0) {
							printf(", ");
						}
						printf("[ %f, %f ]", ops[i].lon, ops[i].lat);
					}
					printf(" ]");
				} else {
					printf("\"type\": \"MultiLineString\", \"coordinates\": [ [ ");
					int state = 0;
					for (size_t i = 0; i < ops.size(); i++) {
						if (ops[i].op == VT_MOVETO) {
							if (state == 0) {
								printf("[ %f, %f ]", ops[i].lon, ops[i].lat);
								state = 1;
							} else {
								printf(" ], [ ");
								printf("[ %f, %f ]", ops[i].lon, ops[i].lat);
								state = 1;
							}
						} else {
							printf(", [ %f, %f ]", ops[i].lon, ops[i].lat);
						}
					}
					printf(" ] ]");
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

					// printf("area %f\n", area / .00000274 / .00000274);
				}

				if (outer > 1) {
					printf("\"type\": \"MultiPolygon\", \"coordinates\": [ [ [ ");
				} else {
					printf("\"type\": \"Polygon\", \"coordinates\": [ [ ");
				}

				int state = 0;
				for (size_t i = 0; i < rings.size(); i++) {
					if (areas[i] >= 0) {
						if (state != 0) {
							// new multipolygon
							printf(" ] ], [ [ ");
						}
						state = 1;
					}

					if (state == 2) {
						// new ring in the same polygon
						printf(" ], [ ");
					}

					for (size_t j = 0; j < rings[i].size(); j++) {
						if (rings[i][j].op != VT_CLOSEPATH) {
							if (j != 0) {
								printf(", ");
							}

							printf("[ %f, %f ]", rings[i][j].lon, rings[i][j].lat);
						} else {
							if (j != 0) {
								printf(", ");
							}

							printf("[ %f, %f ]", rings[i][0].lon, rings[i][0].lat);
						}
					}

					state = 2;
				}

				if (outer > 1) {
					printf(" ] ] ]");
				} else {
					printf(" ] ]");
				}
			}

			printf(" } }\n");
		}

		if (describe) {
			printf("] }\n");
		}
	}

	printf("] }\n");
}

void decode(char *fname, int z, unsigned x, unsigned y) {
	sqlite3 *db;
	int oz = z;
	unsigned ox = x, oy = y;

	int fd = open(fname, O_RDONLY);
	if (fd >= 0) {
		struct stat st;
		if (fstat(fd, &st) == 0) {
			if (st.st_size < 50 * 1024 * 1024) {
				char *map = (char *) mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
				if (map != NULL && map != MAP_FAILED) {
					if (strcmp(map, "SQLite format 3") != 0) {
						if (z >= 0) {
							std::string s = std::string(map, st.st_size);
							handle(s, z, x, y, 1);
							munmap(map, st.st_size);
							return;
						} else {
							fprintf(stderr, "Must specify zoom/x/y to decode a single pbf file\n");
							exit(EXIT_FAILURE);
						}
					}
				}
				munmap(map, st.st_size);
			}
		} else {
			perror("fstat");
		}
		close(fd);
	} else {
		perror(fname);
	}

	if (sqlite3_open(fname, &db) != SQLITE_OK) {
		fprintf(stderr, "%s: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	if (z < 0) {
		printf("{ \"type\": \"FeatureCollection\", \"properties\": {\n");

		const char *sql2 = "SELECT name, value from metadata order by name;";
		sqlite3_stmt *stmt2;
		if (sqlite3_prepare_v2(db, sql2, -1, &stmt2, NULL) != SQLITE_OK) {
			fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}

		int within = 0;
		while (sqlite3_step(stmt2) == SQLITE_ROW) {
			if (within) {
				printf(",\n");
			}
			within = 1;

			const unsigned char *name = sqlite3_column_text(stmt2, 0);
			const unsigned char *value = sqlite3_column_text(stmt2, 1);

			printq((char *) name);
			printf(": ");
			printq((char *) value);
		}

		sqlite3_finalize(stmt2);

		const char *sql = "SELECT tile_data, zoom_level, tile_column, tile_row from tiles order by zoom_level, tile_column, tile_row;";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
			fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}

		printf("\n}, \"features\": [\n");

		within = 0;
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			if (within) {
				printf(",\n");
			}
			within = 1;

			int len = sqlite3_column_bytes(stmt, 0);
			int tz = sqlite3_column_int(stmt, 1);
			int tx = sqlite3_column_int(stmt, 2);
			int ty = sqlite3_column_int(stmt, 3);
			ty = (1LL << tz) - 1 - ty;
			const char *s = (const char *) sqlite3_column_blob(stmt, 0);

			handle(std::string(s, len), tz, tx, ty, 1);
		}

		printf("] }\n");

		sqlite3_finalize(stmt);
	} else {
		int handled = 0;
		while (z >= 0 && !handled) {
			const char *sql = "SELECT tile_data from tiles where zoom_level = ? and tile_column = ? and tile_row = ?;";
			sqlite3_stmt *stmt;
			if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
				fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
				exit(EXIT_FAILURE);
			}

			sqlite3_bind_int(stmt, 1, z);
			sqlite3_bind_int(stmt, 2, x);
			sqlite3_bind_int(stmt, 3, (1LL << z) - 1 - y);

			while (sqlite3_step(stmt) == SQLITE_ROW) {
				int len = sqlite3_column_bytes(stmt, 0);
				const char *s = (const char *) sqlite3_column_blob(stmt, 0);

				if (z != oz) {
					fprintf(stderr, "%s: Warning: using tile %d/%u/%u instead of %d/%u/%u\n", fname, z, x, y, oz, ox, oy);
				}

				handle(std::string(s, len), z, x, y, 0);
				handled = 1;
			}

			sqlite3_finalize(stmt);

			z--;
			x /= 2;
			y /= 2;
		}
	}

	if (sqlite3_close(db) != SQLITE_OK) {
		fprintf(stderr, "%s: could not close database: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s [-t projection] file.mbtiles zoom x y\n", argv[0]);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	extern int optind;
	extern char *optarg;
	int i;

	while ((i = getopt(argc, argv, "t:")) != -1) {
		switch (i) {
		case 't':
			set_projection_or_exit(optarg);
			break;

		default:
			usage(argv);
		}
	}

	if (argc == optind + 4) {
		decode(argv[optind], atoi(argv[optind + 1]), atoi(argv[optind + 2]), atoi(argv[optind + 3]));
	} else if (argc == optind + 1) {
		decode(argv[optind], -1, -1, -1);
	} else {
		usage(argv);
	}

	return 0;
}
