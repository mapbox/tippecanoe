#include <iostream>
#include <fstream>
#include <string>
#include <zlib.h>
#include "vector_tile.pb.h"

extern "C" {
	#include "tile.h"
}

#define XMAX 4096
#define YMAX 4096

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
static inline int compress(std::string const& input, std::string& output) {
	z_stream deflate_s;
	deflate_s.zalloc = Z_NULL;
	deflate_s.zfree = Z_NULL;
	deflate_s.opaque = Z_NULL;
	deflate_s.avail_in = 0;
	deflate_s.next_in = Z_NULL;
	deflateInit(&deflate_s, Z_DEFAULT_COMPRESSION);
	deflate_s.next_in = (Bytef *)input.data();
	deflate_s.avail_in = input.size();
	size_t length = 0;
	do {
		size_t increase = input.size() / 2 + 1024;
		output.resize(length + increase);
		deflate_s.avail_out = increase;
		deflate_s.next_out = (Bytef *)(output.data() + length);
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


void check_range(struct index *start, struct index *end, char *metabase, unsigned *file_bbox) {
	GOOGLE_PROTOBUF_VERIFY_VERSION;

        mapnik::vector::tile tile;
	mapnik::vector::tile_layer *layer = tile.add_layers();

        layer->set_name("name");
        layer->set_version(1);
        layer->set_extent(XMAX);

	struct pool keys;
	keys.n = 0;
	keys.vals = NULL;

	struct index *i;
	printf("tile -----------------------------------------------\n");
	for (i = start; i < end; i++) {
		mapnik::vector::tile_feature *feature = layer->add_features();

		printf("%llx ", i->index);

		char *meta = metabase + i->fpos;

		int t;
		deserialize_int(&meta, &t);
		printf("(%d) ", t);

		if (t == VT_POINT) {
			feature->set_type(mapnik::vector::tile::Point);
		} else if (t == VT_LINE) {
			feature->set_type(mapnik::vector::tile::LineString);
		} else if (t == VT_POLYGON) {
			feature->set_type(mapnik::vector::tile::Polygon);
		} else {
			feature->set_type(mapnik::vector::tile::Unknown);
		}

		while (1) {
			int op;
			deserialize_int(&meta, &op);

			if (op == VT_END) {
				break;
			}

			printf("%d: ", op);

			if (op == VT_MOVETO || op == VT_LINETO) {
				int x, y;
				deserialize_int(&meta, &x);
				deserialize_int(&meta, &y);

				//double lat, lon;
				//tile2latlon(x, y, 32, &lat,&lon);
				//printf("%f,%f (%x/%x) ", lat, lon, x, y);
			}
		}

		int m;
		deserialize_int(&meta, &m);

		int i;
		for (i = 0; i < m; i++) {
			int t;
			deserialize_int(&meta, &t);
			struct pool_val *key = deserialize_string(&meta, &keys, VT_STRING);
			struct pool_val *value = deserialize_string(&meta, &keys, t);

			feature->add_tags(key->n);
			feature->add_tags(value->n);

			printf("%s (%d) = %s (%d)\n", key->s, key->n, value->s, value->n);
		}

		printf("\n");
	}

	struct pool_val *pv;
	for (pv = keys.vals; pv != NULL; pv = pv->next) {
		layer->add_keys(pv->s, strlen(pv->s));
	}
	pool_free(&keys);

        std::string s;
        std::string compressed;

        tile.SerializeToString(&s);
	std::cout << s;
#if 0
        compress(s, compressed);
        std::cout << compressed;
#endif
}

