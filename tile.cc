#include <iostream>
#include <fstream>
#include <string>
#include <stack>
#include <stdio.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <sqlite3.h>
#include "vector_tile.pb.h"

extern "C" {
	#include "tile.h"
}

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

struct draw {
	int op;
	long long x;
	long long y;
	int necessary;
};

int decode_feature(char **meta, struct draw *out, int z, unsigned tx, unsigned ty, int detail) {
	int len = 0;

	while (1) {
		int op;
		deserialize_int(meta, &op);

		if (op == VT_END) {
			break;
		}

		if (out != NULL) {
			out[len].op = op;
		}

		if (op == VT_MOVETO || op == VT_LINETO) {
			int wx, wy;
			deserialize_int(meta, &wx);
			deserialize_int(meta, &wy);

			long long wwx = (unsigned) wx;
			long long wwy = (unsigned) wy;

			if (z != 0) {
				wwx -= tx << (32 - z);
				wwy -= ty << (32 - z);
			}

			if (out != NULL) {
				out[len].x = wwx;
				out[len].y = wwy;
			}
		}

		len++;
	}

	return len;
}

int draw(struct draw *geom, int n, mapnik::vector::tile_feature *feature) {
	int px = 0, py = 0;
	int cmd_idx = -1;
	int cmd = -1;
	int length = 0;
	int drew = 0;
	int i;

	for (i = 0; i < n; i++) {
		int op = geom[i].op;

		if (op != cmd) {
			if (cmd_idx >= 0) {
				if (feature != NULL) {
					feature->set_geometry(cmd_idx, (length << CMD_BITS) | (cmd & ((1 << CMD_BITS) - 1)));
				}
			}

			cmd = op;
			length = 0;

			if (feature != NULL) {
				cmd_idx = feature->geometry_size();
				feature->add_geometry(0);
			}
		}

		if (op == VT_MOVETO || op == VT_LINETO) {
			long long wwx = geom[i].x;
			long long wwy = geom[i].y;

			int dx = wwx - px;
			int dy = wwy - py;

			if (feature != NULL) {
				feature->add_geometry((dx << 1) ^ (dx >> 31));
				feature->add_geometry((dy << 1) ^ (dy >> 31));
			}

			px = wwx;
			py = wwy;
			length++;

			if (op == VT_LINETO && (dx != 0 || dy != 0)) {
				drew = 1;
			}
		} else if (op == VT_CLOSEPATH) {
			length++;
		}
	}

	if (cmd_idx >= 0) {
		if (feature != NULL) {
			feature->set_geometry(cmd_idx, (length << CMD_BITS) | (cmd & ((1 << CMD_BITS) - 1)));
		}
	}

	return drew;
}

int remove_noop(struct draw *geom, int n, int type) {
	// first pass: remove empty linetos

	long long x = 0, y = 0;
	int out = 0;
	int i;

	for (i = 0; i < n; i++) {
		if (geom[i].op == VT_LINETO && geom[i].x == x && geom[i].y == y) {
			continue;
		}

		if (geom[i].op == VT_CLOSEPATH) {
			geom[out++] = geom[i];
		} else { /* moveto or lineto */
			geom[out++] = geom[i];
			x = geom[i].x;
			y = geom[i].y;
		}
	}

	// second pass: remove unused movetos

	n = out;
	out = 0;

	for (i = 0; i < n; i++) {
		if (geom[i].op == VT_MOVETO) {
			if (i + 1 >= n) {
				continue;
			}

			if (geom[i + 1].op == VT_MOVETO) {
				continue;
			}

			if (geom[i + 1].op == VT_CLOSEPATH) {
				i++; // also remove unused closepath
				continue;
			}
		}

		geom[out++] = geom[i];
	}

	// second pass: remove empty movetos

	if (type == VT_LINE) {
		n = out;
		out = 0;

		for (i = 0; i < n; i++) {
			if (geom[i].op == VT_MOVETO) {
				if (i - 1 >= 0 && geom[i - 1].op == VT_LINETO && geom[i - 1].x == geom[i].x && geom[i - 1].y == geom[i].y) {
					continue;
				}
			}

			geom[out++] = geom[i];
		}
	}

	return out;
}

int shrink_lines(struct draw *geom, int len, int z, int basezoom) {
	double scale = 1.0 / exp(log(sqrt(2.5)) * (basezoom - z));
	struct draw tmp[3 * len];
	int out = 0;
	int i;

	for (i = 0; i < len; i++) {
		if (i > 0 && (geom[i - 1].op == VT_MOVETO || geom[i - 1].op == VT_LINETO) && geom[i].op == VT_LINETO) {
			long long cx = (geom[i].x + geom[i - 1].x) / 2;
			long long cy = (geom[i].y + geom[i - 1].y) / 2;

			tmp[out + 0].op = VT_MOVETO;
			tmp[out + 0].x = cx + (geom[i - 1].x - cx) * scale;
			tmp[out + 0].y = cy + (geom[i - 1].y - cy) * scale;

			tmp[out + 1].op = VT_LINETO;
			tmp[out + 1].x = cx + (geom[i].x - cx) * scale;
			tmp[out + 1].y = cy + (geom[i].y - cy) * scale;

			tmp[out + 2].op = VT_MOVETO;
			tmp[out + 2].x = geom[i].x;
			tmp[out + 2].y = geom[i].y;

			out += 3;
		} else {
			tmp[out++] = geom[i];
		}
	}

	memcpy(geom, tmp, out * sizeof(struct draw));
	return out;
}

void to_tile_scale(struct draw *geom, int n, int z, int detail) {
	int i;

	for (i = 0; i < n; i++) {
		geom[i].x >>= (32 - detail - z);
		geom[i].y >>= (32 - detail - z);
	}
}

double square_distance_from_line(long long point_x, long long point_y, long long segA_x, long long segA_y, long long segB_x, long long segB_y) {
	double p2x = segB_x - segA_x;
	double p2y = segB_y - segA_y;
	double something = p2x * p2x + p2y * p2y;
	double u = 0 == something ? 0 : ((point_x - segA_x) * p2x + (point_y - segA_y) * p2y) / something;

	if (u > 1) {
		u = 1;
	} else if (u < 0) {
		u = 0;
	}

	double x = segA_x + u * p2x;
	double y = segA_y + u * p2y;

	double dx = x - point_x;
	double dy = y - point_y;

	return dx * dx + dy * dy;
}

// https://github.com/Project-OSRM/osrm-backend/blob/733d1384a40f/Algorithms/DouglasePeucker.cpp
void douglas_peucker(struct draw *geom, int n, double e) {
	e = e * e;
	std::stack<int> recursion_stack;

	{
		int left_border = 0;
		int right_border = 1;
		// Sweep linerarily over array and identify those ranges that need to be checked
		do {
			if (geom[right_border].necessary) {
				recursion_stack.push(left_border);
				recursion_stack.push(right_border);
				left_border = right_border;
			}
			++right_border;
		} while (right_border < n);
	}

	while (!recursion_stack.empty()) {
		// pop next element
		int second = recursion_stack.top();
		recursion_stack.pop();
		int first = recursion_stack.top();
		recursion_stack.pop();

		double max_distance = -1;
		int farthest_element_index = second;

		// find index idx of element with max_distance
		int i;
		for (i = first + 1; i < second; i++) {
			double temp_dist = square_distance_from_line(geom[i].x, geom[i].y,
				geom[first].x, geom[first].y,
				geom[second].x, geom[second].y);

			double distance = fabs(temp_dist);

			if (distance > e && distance > max_distance) {
				farthest_element_index = i;
				max_distance = distance;
			}
		}

		if (max_distance > e) {
			// mark idx as necessary
			geom[farthest_element_index].necessary = 1;

			if (1 < farthest_element_index - first) {
				recursion_stack.push(first);
				recursion_stack.push(farthest_element_index);
			}
			if (1 < second - farthest_element_index) {
				recursion_stack.push(farthest_element_index);
				recursion_stack.push(second);
			}
		}
	}
}

int simplify_lines(struct draw *geom, int n, int z, int detail) {
	int res = 1 << (32 - 10 - z); // 1/4 pixel

	int i;
	for (i = 0; i < n; i++) {
		if (geom[i].op == VT_MOVETO) {
			geom[i].necessary = 1;
		} else if (geom[i].op == VT_LINETO) {
			geom[i].necessary = 0;
		} else {
			geom[i].necessary = 1;
		}
	}

	for (i = 0; i < n; i++) {
		if (geom[i].op == VT_MOVETO) {
			int j;
			for (j = i + 1; j < n; j++) {
				if (geom[j].op == VT_CLOSEPATH || geom[j].op == VT_MOVETO) {
					break;
				}
			}

			geom[i].necessary = 1;
			geom[j - 1].necessary = 1;

			douglas_peucker(geom + i, j - i, res);
			i = j - 1;
		}
	}

	int out = 0;
	for (i = 0; i < n; i++) {
		if (geom[i].necessary) {
			geom[out++] = geom[i];
		}
	}

	return out;
}

long long write_tile(struct index *start, struct index *end, char *metabase, unsigned *file_bbox, int z, unsigned tx, unsigned ty, int detail, int basezoom, struct pool *file_keys, char *layername, sqlite3 *outdb) {
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	mapnik::vector::tile tile;
	mapnik::vector::tile_layer *layer = tile.add_layers();

	layer->set_name(layername);
	layer->set_version(1);

	layer->set_extent(1 << detail);

	struct pool keys;
	keys.n = 0;
	keys.vals = NULL;
	keys.head = NULL;
	keys.tail = NULL;

	struct pool values;
	values.n = 0;
	values.vals = NULL;
	values.head = NULL;
	values.tail = NULL;

	struct pool dup;
	dup.n = 1;
	dup.vals = NULL;
	dup.head = NULL;
	dup.tail = NULL;

	double interval = 1;
	double seq = 0;
	long long count = 0;

	if (z < basezoom) {
		interval = exp(log(2.5) * (basezoom - z));
	}

	struct index *i;
	for (i = start; i < end; i++) {
		int t;

		char *meta = metabase + i->fpos;
		deserialize_int(&meta, &t);

		if (t == VT_POINT) {
			seq++;

			if (seq >= 0) {
				seq -= interval;
			} else {
				continue;
			}
		}

		int len = decode_feature(&meta, NULL, z, tx, ty, detail);
		struct draw geom[3 * len];

		meta = metabase + i->fpos;
		deserialize_int(&meta, &t);
		decode_feature(&meta, geom, z, tx, ty, detail);

		if (t == VT_LINE || t == VT_POLYGON) {
			len = simplify_lines(geom, len, z, detail);
		}

#if 0
		if (t == VT_LINE && z != basezoom) {
			len = shrink_lines(geom, len, z, basezoom);
		}
#endif

		to_tile_scale(geom, len, z, detail);

		if (t == VT_LINE || t == VT_POLYGON) {
			len = remove_noop(geom, len, t);
		}

		if (t == VT_POINT || draw(geom, len, NULL)) {
			struct pool_val *pv = pool_long_long(&dup, &i->fpos, 0);
			if (pv->n == 0) {
				continue;
			}
			pv->n = 0;

			mapnik::vector::tile_feature *feature = layer->add_features();

			if (t == VT_POINT) {
				feature->set_type(mapnik::vector::tile::Point);
			} else if (t == VT_LINE) {
				feature->set_type(mapnik::vector::tile::LineString);
			} else if (t == VT_POLYGON) {
				feature->set_type(mapnik::vector::tile::Polygon);
			} else {
				feature->set_type(mapnik::vector::tile::Unknown);
			}

			draw(geom, len, feature);
			count += len;

			int m;
			deserialize_int(&meta, &m);

			int i;
			for (i = 0; i < m; i++) {
				int t;
				deserialize_int(&meta, &t);
				struct pool_val *key = deserialize_string(&meta, &keys, VT_STRING);
				struct pool_val *value = deserialize_string(&meta, &values, t);

				feature->add_tags(key->n);
				feature->add_tags(value->n);

				// Dup to retain after munmap
				pool(file_keys, strdup(key->s), t);
			}
		}
	}

	struct pool_val *pv;
	for (pv = keys.head; pv != NULL; pv = pv->next) {
		layer->add_keys(pv->s, strlen(pv->s));
	}
	for (pv = values.head; pv != NULL; pv = pv->next) {
		mapnik::vector::tile_value *tv = layer->add_values();

		if (pv->type == VT_NUMBER) {
			tv->set_double_value(atof(pv->s));
		} else {
			tv->set_string_value(pv->s);
		}
	}
	pool_free(&keys);
	pool_free(&values);
	pool_free(&dup);

	std::string s;
	std::string compressed;

	tile.SerializeToString(&s);
	compress(s, compressed);

	if (compressed.size() > 500000) {
		fprintf(stderr, "tile %d/%u/%u size is %lld, >500000\n", z, tx, ty, (long long) compressed.size());
		exit(EXIT_FAILURE);
	}

	sqlite3_stmt *stmt;
	const char *query = "insert into tiles (zoom_level, tile_column, tile_row, tile_data) values (?, ?, ?, ?)";
	if (sqlite3_prepare_v2(outdb, query, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "sqlite3 insert prep failed\n");
		exit(EXIT_FAILURE);
	}

	sqlite3_bind_int(stmt, 1, z);
	sqlite3_bind_int(stmt, 2, tx);
	sqlite3_bind_int(stmt, 3, (1 << z) - 1 - ty);
	sqlite3_bind_blob(stmt, 4, compressed.data(), compressed.size(), NULL);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		fprintf(stderr, "sqlite3 insert failed: %s\n", sqlite3_errmsg(outdb));
	}

	return count;
}

