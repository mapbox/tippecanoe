#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string>
#include <zlib.h>
#include <math.h>
#include "vector_tile.pb.h"

extern "C" {
#include "projection.h"
#include "pool.h"
}

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
inline bool is_compressed(std::string const &data) {
	return data.size() > 2 && (((uint8_t) data[0] == 0x78 && (uint8_t) data[1] == 0x9C) || ((uint8_t) data[0] == 0x1F && (uint8_t) data[1] == 0x8B));
}

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
inline int decompress(std::string const &input, std::string &output) {
	z_stream inflate_s;
	inflate_s.zalloc = Z_NULL;
	inflate_s.zfree = Z_NULL;
	inflate_s.opaque = Z_NULL;
	inflate_s.avail_in = 0;
	inflate_s.next_in = Z_NULL;
	if (inflateInit2(&inflate_s, 32 + 15) != Z_OK) {
		fprintf(stderr, "error: %s\n", inflate_s.msg);
	}
	inflate_s.next_in = (Bytef *) input.data();
	inflate_s.avail_in = input.size();
	size_t length = 0;
	do {
		output.resize(length + 2 * input.size());
		inflate_s.avail_out = 2 * input.size();
		inflate_s.next_out = (Bytef *) (output.data() + length);
		int ret = inflate(&inflate_s, Z_FINISH);
		if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR) {
			fprintf(stderr, "error: %s\n", inflate_s.msg);
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

void handle(std::string message, int z, unsigned x, unsigned y, struct pool *file_keys) {
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// https://github.com/mapbox/mapnik-vector-tile/blob/master/examples/c%2B%2B/tileinfo.cpp
	mapnik::vector::tile tile;
	mapnik::vector::tile outtile;

	if (is_compressed(message)) {
		std::string uncompressed;
		decompress(message, uncompressed);
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
		mapnik::vector::tile_layer *outlayer = outtile.add_layers();

		outlayer->set_name(layer.name());
		outlayer->set_version(layer.version());
		outlayer->set_extent(layer.extent());

		for (int f = 0; f < layer.features_size(); f++) {
			mapnik::vector::tile_feature feat = layer.features(f);
			mapnik::vector::tile_feature *outfeature = outlayer->add_features();

			outfeature->set_type(feat.type());

			for (int g = 0; g < feat.geometry_size(); g++) {
				outfeature->add_geometry(feat.geometry(g));
			}
		}
	}
}

void decode(char *fname, char *map, struct pool *file_keys) {
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

		printf("found %lld/%lld/%lld\n", zoom, x, y);

		handle(std::string(s, len), zoom, x, y, file_keys);
	}

	sqlite3_finalize(stmt);

	if (sqlite3_close(db) != SQLITE_OK) {
		fprintf(stderr, "%s: could not close database: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s [-f] -o new.mbtiles source.mbtiles map.csv\n", argv[0]);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	char *outfile = NULL;
	int force = 0;

	extern int optind;
	extern char *optarg;
	int i;

	while ((i = getopt(argc, argv, "fo:")) != -1) {
		switch (i) {
			case 'o':
				outfile = optarg;
				break;

			case 'f':
				force = 1;
				break;

			default:
				usage(argv);
		}
	}

	if (argc != optind + 2 || outfile == NULL) {
		usage(argv);
	}

	struct pool file_keys;
	pool_init(&file_keys, 0);

	decode(argv[optind], argv[optind + 1], &file_keys);

	pool_free(&file_keys);

	return 0;
}
