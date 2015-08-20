// for vasprintf() on Linux
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string>
#include <zlib.h>
#include <math.h>
#include "vector_tile.pb.h"
#include "tile.h"

extern "C" {
#include "projection.h"
#include "pool.h"
#include "mbtiles.h"
}

struct stats {
	int minzoom;
	int maxzoom;
	double midlat, midlon;
	double minlat, minlon, maxlat, maxlon;
};

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

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
static inline int compress(std::string const &input, std::string &output) {
	z_stream deflate_s;
	deflate_s.zalloc = Z_NULL;
	deflate_s.zfree = Z_NULL;
	deflate_s.opaque = Z_NULL;
	deflate_s.avail_in = 0;
	deflate_s.next_in = Z_NULL;
	deflateInit2(&deflate_s, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
	deflate_s.next_in = (Bytef *) input.data();
	deflate_s.avail_in = input.size();
	size_t length = 0;
	do {
		size_t increase = input.size() / 2 + 1024;
		output.resize(length + increase);
		deflate_s.avail_out = increase;
		deflate_s.next_out = (Bytef *) (output.data() + length);
		int ret = deflate(&deflate_s, Z_FINISH);
		if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR) {
			return -1;
		}
		length += (increase - deflate_s.avail_out);
	} while (deflate_s.avail_out == 0);
	deflateEnd(&deflate_s);
	output.resize(length);
	return 0;
}

void handle(std::string message, int z, unsigned x, unsigned y, struct pool **file_keys, char ***layernames, int *nlayers, sqlite3 *outdb) {
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

		const char *ln = layer.name().c_str();

		int ll;
		for (ll = 0; ll < *nlayers; ll++) {
			if (strcmp((*layernames)[ll], ln) == 0) {
				break;
			}
		}
		if (ll == *nlayers) {
			*file_keys = (struct pool *) realloc(*file_keys, (ll + 1) * sizeof(struct pool));
			*layernames = (char **) realloc(*layernames, (ll + 1) * sizeof(char *));

			if (*file_keys == NULL) {
				perror("realloc file_keys");
				exit(EXIT_FAILURE);
			}
			if (*layernames == NULL) {
				perror("realloc layernames");
				exit(EXIT_FAILURE);
			}

			pool_init(&((*file_keys)[ll]), 0);
			(*layernames)[ll] = strdup(ln);
			*nlayers = ll + 1;
		}

		struct pool keys, values;
		pool_init(&keys, 0);
		pool_init(&values, 0);

		for (int f = 0; f < layer.features_size(); f++) {
			mapnik::vector::tile_feature feat = layer.features(f);
			mapnik::vector::tile_feature *outfeature = outlayer->add_features();

			outfeature->set_type(feat.type());

			for (int g = 0; g < feat.geometry_size(); g++) {
				outfeature->add_geometry(feat.geometry(g));
			}

			for (int t = 0; t + 1 < feat.tags_size(); t += 2) {
				if (feat.tags(t) >= layer.keys_size() || feat.tags(t + 1) >= layer.values_size()) {
					printf("out of range: %d=%d\n", feat.tags(t), feat.tags(t + 1));
					continue;
				}

				const char *key = layer.keys(feat.tags(t)).c_str();
				mapnik::vector::tile_value const &val = layer.values(feat.tags(t + 1));
				char *value;
				int type;

				if (val.has_string_value()) {
					value = strdup(val.string_value().c_str());
					type = VT_STRING;
				} else if (val.has_int_value()) {
					asprintf(&value, "%lld", val.int_value());
					type = VT_NUMBER;
				} else if (val.has_double_value()) {
					asprintf(&value, "%g", val.double_value());
					type = VT_NUMBER;
				} else if (val.has_float_value()) {
					asprintf(&value, "%g", val.float_value());
					type = VT_NUMBER;
				} else if (val.has_bool_value()) {
					asprintf(&value, "%s", val.bool_value() ? "true" : "false");
					type = VT_BOOLEAN;
				} else if (val.has_sint_value()) {
					asprintf(&value, "%lld", val.sint_value());
					type = VT_NUMBER;
				} else if (val.has_uint_value()) {
					asprintf(&value, "%llu", val.uint_value());
					type = VT_NUMBER;
				} else {
					continue;
				}

				if (!is_pooled(&((*file_keys)[ll]), key, type)) {
					pool(&((*file_keys)[ll]), strdup(key), type);
				}

				struct pool_val *k, *v;

				if (is_pooled(&keys, key, VT_STRING)) {
					k = pool(&keys, key, VT_STRING);
				} else {
					k = pool(&keys, strdup(key), VT_STRING);
				}

				if (is_pooled(&values, value, type)) {
					v = pool(&values, value, type);
				} else {
					v = pool(&values, strdup(value), type);
				}

				outfeature->add_tags(k->n);
				outfeature->add_tags(v->n);
				free(value);
			}
		}

		struct pool_val *pv;
		for (pv = keys.head; pv != NULL; pv = pv->next) {
			outlayer->add_keys(pv->s, strlen(pv->s));
		}
		for (pv = values.head; pv != NULL; pv = pv->next) {
			mapnik::vector::tile_value *tv = outlayer->add_values();

			if (pv->type == VT_NUMBER) {
				tv->set_double_value(atof(pv->s));
			} else if (pv->type == VT_BOOLEAN) {
				tv->set_bool_value(pv->s[0] == 't');
			} else {
				tv->set_string_value(pv->s);
			}
		}

		pool_free_strings(&keys);
		pool_free_strings(&values);
	}

	std::string s;
	std::string compressed;

	outtile.SerializeToString(&s);
	compress(s, compressed);

	mbtiles_write_tile(outdb, z, x, y, compressed.data(), compressed.size());
}

void decode(char *fname, char *map, struct pool **file_keys, char ***layernames, int *nlayers, sqlite3 *outdb, struct stats *st) {
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

		handle(std::string(s, len), zoom, x, y, file_keys, layernames, nlayers, outdb);
	}

	sqlite3_finalize(stmt);

	if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'minzoom'", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			st->minzoom = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'maxzoom'", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			st->maxzoom = sqlite3_column_int(stmt, 0);
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
	if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'bounds'", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			const unsigned char *s = sqlite3_column_text(stmt, 0);
			sscanf((char *) s, "%lf,%lf,%lf,%lf", &st->minlon, &st->minlat, &st->maxlon, &st->maxlat);
		}
		sqlite3_finalize(stmt);
	}

	if (sqlite3_close(db) != SQLITE_OK) {
		fprintf(stderr, "%s: could not close database: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s [-f] [-c joins.csv] -o new.mbtiles source.mbtiles\n", argv[0]);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	char *outfile = NULL;
	char *csv = NULL;
	int force = 0;

	extern int optind;
	extern char *optarg;
	int i;

	while ((i = getopt(argc, argv, "fo:c:")) != -1) {
		switch (i) {
			case 'o':
				outfile = optarg;
				break;

			case 'f':
				force = 1;
				break;

			case 'c':
				csv = optarg;
				break;

			default:
				usage(argv);
		}
	}

	if (argc - optind != 1 || outfile == NULL) {
		usage(argv);
	}

	if (force) {
		unlink(outfile);
	}

	sqlite3 *outdb = mbtiles_open(outfile, argv);
	struct stats st;
	memset(&st, 0, sizeof(st));

	struct pool *file_keys = NULL;
	char **layernames = NULL;
	int nlayers = 0;

	for (i = optind; i < argc; i++) {
		decode(argv[i], csv, &file_keys, &layernames, &nlayers, outdb, &st);
	}

	for (i = 0; i < nlayers; i++) {
		printf("%s\n", layernames[i]);
	}

	struct pool *fk[nlayers];
	for (i = 0; i < nlayers; i++) {
		fk[i] = &(file_keys[i]);
	}

	mbtiles_write_metadata(outdb, outfile, layernames, st.minzoom, st.maxzoom, st.minlat, st.minlon, st.maxlat, st.maxlon, st.midlat, st.midlon, fk, nlayers);
	mbtiles_close(outdb, argv);

	return 0;
}
