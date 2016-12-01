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
#include "plugin.hpp"

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
			fprintq(stdout, projection->alias);
			printf(" } }");
		}
	}

	printf(", \"features\": [\n");

	for (size_t l = 0; l < tile.layers.size(); l++) {
		mvt_layer &layer = tile.layers[l];

		if (describe) {
			if (l != 0) {
				printf(",\n");
			}

			printf("{ \"type\": \"FeatureCollection\"");
			printf(", \"properties\": { \"layer\": ");
			fprintq(stdout, layer.name.c_str());
			printf(", \"version\": %d, \"extent\": %d", layer.version, layer.extent);
			printf(" }");
			printf(", \"features\": [\n");

			within = 0;
		}

		layer_to_geojson(stdout, layer, z, x, y);

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

			fprintq(stdout, (char *) name);
			printf(": ");
			fprintq(stdout, (char *) value);
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
