#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string>
#include <zlib.h>
#include <math.h>
#include "vector_tile.pb.h"

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
inline bool is_compressed(std::string const& data) {
	return data.size() > 2 && (uint8_t)data[0] == 0x78 && (uint8_t)data[1] == 0x9C;
}

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
inline int decompress(std::string const& input, std::string & output) {
	z_stream inflate_s;
	inflate_s.zalloc = Z_NULL;
	inflate_s.zfree = Z_NULL;
	inflate_s.opaque = Z_NULL;
	inflate_s.avail_in = 0;
	inflate_s.next_in = Z_NULL;
	inflateInit(&inflate_s);
	inflate_s.next_in = (Bytef *)input.data();
	inflate_s.avail_in = input.size();
	size_t length = 0;
	do {
		output.resize(length + 2 * input.size());
		inflate_s.avail_out = 2 * input.size();
		inflate_s.next_out = (Bytef *)(output.data() + length);
		int ret = inflate(&inflate_s, Z_FINISH);
		if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR) {
			return 0;
		}

		length += (2 * input.size() - inflate_s.avail_out);
	} while (inflate_s.avail_out == 0);
	inflateEnd(&inflate_s);
	output.resize(length);
	return 1;
}

int dezig(unsigned n) {
	return (n >> 1) ^ (-(n & 1));
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void tile2latlon(unsigned int x, unsigned int y, int zoom, double *lat, double *lon) {
        unsigned long long n = 1LL << zoom;
        *lon = 360.0 * x / n - 180.0;
        double lat_rad = atan(sinh(M_PI * (1 - 2.0 * y / n)));
        *lat = lat_rad * 180 / M_PI;
}

void handle(std::string message, int z, unsigned x, unsigned y) {
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// https://github.com/mapbox/mapnik-vector-tile/blob/master/examples/c%2B%2B/tileinfo.cpp
	mapnik::vector::tile tile;

	if (is_compressed(message)) {
		std::string uncompressed;
		decompress(message,uncompressed);
		if (!tile.ParseFromString(uncompressed)) {
			fprintf(stderr, "Couldn't decompress tile %d/%u/%u\n", z, x, y);
			exit(EXIT_FAILURE);
		}
	} else if (!tile.ParseFromString(message)) {
		fprintf(stderr, "Couldn't parse tile %d/%u/%u\n", z, x, y);
		exit(EXIT_FAILURE);
	}

	for (int l = 0; l < tile.layers_size(); l++) {
		mapnik::vector::tile_layer layer = tile.layers(l);
		int extent = layer.extent();

		for (int f = 0; f < layer.features_size(); f++) {
			mapnik::vector::tile_feature feat = layer.features(f);
			int px = 0, py = 0;

			for (int g = 0; g < feat.geometry_size(); g++) {
				uint32_t geom = feat.geometry(g);
				uint32_t op = geom & 7;
				uint32_t count = geom >> 3;

				if (op == 1 || op == 2) {
					if (op == 1) {
						printf("\n");
					}

					for (unsigned k = 0; k < count; k++) {
						px += dezig(feat.geometry(g + 1));
						py += dezig(feat.geometry(g + 2));
						g += 2;

						long long scale = 1LL << (32 - z);
						long long wx = scale * x + (scale / extent) * (px + .5);
						long long wy = scale * y + (scale / extent) * (py + .5);

						double lat, lon;
						tile2latlon(wx, wy, 32, &lat, &lon);
						printf("%f,%f ", lat, lon);
					}
				}
			}
		}
	}
}

void decode(char *fname, int z, unsigned x, unsigned y) {
	sqlite3 *db;

	if (sqlite3_open(fname, &db) != SQLITE_OK) {
		fprintf(stderr, "%s: %s\n", fname,  sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

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

		handle(std::string(s, len), z, x, y);
	}

	sqlite3_finalize(stmt);

        if (sqlite3_close(db) != SQLITE_OK) {
                fprintf(stderr, "%s: could not close database: %s\n", fname, sqlite3_errmsg(db));
                exit(EXIT_FAILURE);
        }
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s file.mbtiles zoom x y\n", argv[0]);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	extern int optind;
	// extern char *optarg;
	int i;

	while ((i = getopt(argc, argv, "")) != -1) {
		usage(argv);
	}

	if (argc != optind + 4) {
		usage(argv);
	}

	decode(argv[optind], atoi(argv[optind + 1]), atoi(argv[optind + 2]), atoi(argv[optind + 3]));

	return 0;
}
