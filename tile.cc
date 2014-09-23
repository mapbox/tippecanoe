#include <iostream>
#include <fstream>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "vector_tile.pb.h"

extern "C" {
	#include "tile.h"
}

#define XMAX 4096
#define YMAX 4096

#define CMD_BITS 3

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


void write_tile(struct index *start, struct index *end, char *metabase, unsigned *file_bbox, int z, unsigned tx, unsigned ty) {
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
	//printf("tile -----------------------------------------------\n");
	for (i = start; i < end; i++) {
		mapnik::vector::tile_feature *feature = layer->add_features();

		int px = 0, py = 0;
		int cmd_idx = -1;
		int cmd = -1;
		int length = 0;

		//printf("%llx ", i->index);

		char *meta = metabase + i->fpos;

		int t;
		deserialize_int(&meta, &t);
		//printf("(%d) ", t);

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

			//printf("%d: ", op);

			if (op != cmd) {
				if (cmd_idx >= 0) {
					feature->set_geometry(cmd_idx, (length << CMD_BITS) | (cmd & ((1 << CMD_BITS) - 1)));
				}

				cmd = op;
				length = 0;
				cmd_idx = feature->geometry_size();
				feature->add_geometry(0);
			}

			if (op == VT_MOVETO || op == VT_LINETO) {
				int wx, wy;
				deserialize_int(&meta, &wx);
				deserialize_int(&meta, &wy);

				long long wwx = (unsigned) wx;
				long long wwy = (unsigned) wy;

				wwx -= tx << (32 - z);
				wwy -= ty << (32 - z);

				wwx >>= (32 - 12 - z);
				wwy >>= (32 - 12 - z);

				int dx = wwx - px;
				int dy = wwy - py;

				feature->add_geometry((dx << 1) ^ (dx >> 31));
				feature->add_geometry((dy << 1) ^ (dy >> 31));

				px = wwx;
				py = wwy;
				length++;

				//printf("%lld,%lld ", wwx, wwy);
			} else if (op == VT_CLOSEPATH) {
				length++;
			}
		}

		if (cmd_idx >= 0) {
			feature->set_geometry(cmd_idx, (length << CMD_BITS) | (cmd & ((1 << CMD_BITS) - 1)));
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

			//printf("%s (%d) = %s (%d)\n", key->s, key->n, value->s, value->n);
		}

		//printf("\n");
	}

	struct pool_val *pv;
	for (pv = keys.vals; pv != NULL; pv = pv->next) {
		layer->add_keys(pv->s, strlen(pv->s));
	}
	pool_free(&keys);

        std::string s;
        std::string compressed;

        tile.SerializeToString(&s);
        compress(s, compressed);
        //std::cout << compressed;

	const char *prefix = "tiles";
	char path[strlen(prefix) + 200];

	mkdir(prefix, 0777);

	sprintf(path, "%s/%d", prefix, z);
	mkdir(path, 0777);

	sprintf(path, "%s/%d/%u", prefix, z, tx);
	mkdir(path, 0777);

	sprintf(path, "%s/%d/%u/%u.pbf", prefix, z, tx, ty);
	FILE *f = fopen(path, "wb");
	fwrite(compressed.data(), 1, compressed.size(), f);
	fclose(f);
}

