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

void write_tile(char *name, struct pool *keys) {
	GOOGLE_PROTOBUF_VERIFY_VERSION;

        mapnik::vector::tile tile;
	mapnik::vector::tile_layer *layer = tile.add_layers();

        layer->set_name("name");
        layer->set_version(1);
        layer->set_extent(XMAX);

	struct pool_val *pv;
	for (pv = keys->vals; pv != NULL; pv = pv->next) {
		layer->add_keys(pv->s, strlen(pv->s));
	}




        std::string s;
        std::string compressed;

        tile.SerializeToString(&s);
	std::cout << s;
#if 0
        compress(s, compressed);
        std::cout << compressed;
#endif
}

