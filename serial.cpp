#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <set>
#include <map>
#include <algorithm>
#include <limits.h>
#include "protozero/varint.hpp"
#include "geometry.hpp"
#include "mbtiles.hpp"
#include "tile.hpp"
#include "serial.hpp"
#include "options.hpp"
#include "main.hpp"
#include "pool.hpp"
#include "projection.hpp"
#include "evaluator.hpp"
#include "milo/dtoa_milo.h"

// Offset coordinates to keep them positive
#define COORD_OFFSET (4LL << 32)
#define SHIFT_RIGHT(a) ((((a) + COORD_OFFSET) >> geometry_scale) - (COORD_OFFSET >> geometry_scale))
#define SHIFT_LEFT(a) ((((a) + (COORD_OFFSET >> geometry_scale)) << geometry_scale) - COORD_OFFSET)

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname) {
	size_t w = fwrite(ptr, size, nitems, stream);
	if (w != nitems) {
		fprintf(stderr, "%s: Write to temporary file failed: %s\n", fname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return w;
}

void serialize_int(FILE *out, int n, std::atomic<long long> *fpos, const char *fname) {
	serialize_long_long(out, n, fpos, fname);
}

void serialize_long_long(FILE *out, long long n, std::atomic<long long> *fpos, const char *fname) {
	unsigned long long zigzag = protozero::encode_zigzag64(n);

	serialize_ulong_long(out, zigzag, fpos, fname);
}

void serialize_ulong_long(FILE *out, unsigned long long zigzag, std::atomic<long long> *fpos, const char *fname) {
	while (1) {
		unsigned char b = zigzag & 0x7F;
		if ((zigzag >> 7) != 0) {
			b |= 0x80;
			if (putc(b, out) == EOF) {
				fprintf(stderr, "%s: Write to temporary file failed: %s\n", fname, strerror(errno));
				exit(EXIT_FAILURE);
			}
			*fpos += 1;
			zigzag >>= 7;
		} else {
			if (putc(b, out) == EOF) {
				fprintf(stderr, "%s: Write to temporary file failed: %s\n", fname, strerror(errno));
				exit(EXIT_FAILURE);
			}
			*fpos += 1;
			break;
		}
	}
}

void serialize_byte(FILE *out, signed char n, std::atomic<long long> *fpos, const char *fname) {
	fwrite_check(&n, sizeof(signed char), 1, out, fname);
	*fpos += sizeof(signed char);
}

void serialize_uint(FILE *out, unsigned n, std::atomic<long long> *fpos, const char *fname) {
	fwrite_check(&n, sizeof(unsigned), 1, out, fname);
	*fpos += sizeof(unsigned);
}

void deserialize_int(char **f, int *n) {
	long long ll;
	deserialize_long_long(f, &ll);
	*n = ll;
}

void deserialize_long_long(char **f, long long *n) {
	unsigned long long zigzag = 0;
	deserialize_ulong_long(f, &zigzag);
	*n = protozero::decode_zigzag64(zigzag);
}

void deserialize_ulong_long(char **f, unsigned long long *zigzag) {
	*zigzag = 0;
	int shift = 0;

	while (1) {
		if ((**f & 0x80) == 0) {
			*zigzag |= ((unsigned long long) **f) << shift;
			*f += 1;
			shift += 7;
			break;
		} else {
			*zigzag |= ((unsigned long long) (**f & 0x7F)) << shift;
			*f += 1;
			shift += 7;
		}
	}
}

void deserialize_uint(char **f, unsigned *n) {
	memcpy(n, *f, sizeof(unsigned));
	*f += sizeof(unsigned);
}

void deserialize_byte(char **f, signed char *n) {
	memcpy(n, *f, sizeof(signed char));
	*f += sizeof(signed char);
}

int deserialize_long_long_io(FILE *f, long long *n, std::atomic<long long> *geompos) {
	unsigned long long zigzag = 0;
	int ret = deserialize_ulong_long_io(f, &zigzag, geompos);
	*n = protozero::decode_zigzag64(zigzag);
	return ret;
}

int deserialize_ulong_long_io(FILE *f, unsigned long long *zigzag, std::atomic<long long> *geompos) {
	*zigzag = 0;
	int shift = 0;

	while (1) {
		int c = getc(f);
		if (c == EOF) {
			return 0;
		}
		(*geompos)++;

		if ((c & 0x80) == 0) {
			*zigzag |= ((unsigned long long) c) << shift;
			shift += 7;
			break;
		} else {
			*zigzag |= ((unsigned long long) (c & 0x7F)) << shift;
			shift += 7;
		}
	}

	return 1;
}

int deserialize_int_io(FILE *f, int *n, std::atomic<long long> *geompos) {
	long long ll = 0;
	int ret = deserialize_long_long_io(f, &ll, geompos);
	*n = ll;
	return ret;
}

int deserialize_uint_io(FILE *f, unsigned *n, std::atomic<long long> *geompos) {
	if (fread(n, sizeof(unsigned), 1, f) != 1) {
		return 0;
	}
	*geompos += sizeof(unsigned);
	return 1;
}

int deserialize_byte_io(FILE *f, signed char *n, std::atomic<long long> *geompos) {
	int c = getc(f);
	if (c == EOF) {
		return 0;
	}
	*n = c;
	(*geompos)++;
	return 1;
}

static void write_geometry(drawvec const &dv, std::atomic<long long> *fpos, FILE *out, const char *fname, long long wx, long long wy) {
	for (size_t i = 0; i < dv.size(); i++) {
		if (dv[i].op == VT_MOVETO || dv[i].op == VT_LINETO) {
			serialize_byte(out, dv[i].op, fpos, fname);
			serialize_long_long(out, dv[i].x - wx, fpos, fname);
			serialize_long_long(out, dv[i].y - wy, fpos, fname);
			wx = dv[i].x;
			wy = dv[i].y;
		} else {
			serialize_byte(out, dv[i].op, fpos, fname);
		}
	}
}

// called from generating the next zoom level
void serialize_feature(FILE *geomfile, serial_feature *sf, std::atomic<long long> *geompos, const char *fname, long long wx, long long wy, bool include_minzoom) {
	serialize_byte(geomfile, sf->t, geompos, fname);

	long long layer = 0;
	layer |= sf->layer << 6;
	layer |= (sf->seq != 0) << 5;
	layer |= (sf->index != 0) << 4;
	layer |= (sf->extent != 0) << 3;
	layer |= sf->has_id << 2;
	layer |= sf->has_tippecanoe_minzoom << 1;
	layer |= sf->has_tippecanoe_maxzoom << 0;

	serialize_long_long(geomfile, layer, geompos, fname);
	if (sf->seq != 0) {
		serialize_long_long(geomfile, sf->seq, geompos, fname);
	}
	if (sf->has_tippecanoe_minzoom) {
		serialize_int(geomfile, sf->tippecanoe_minzoom, geompos, fname);
	}
	if (sf->has_tippecanoe_maxzoom) {
		serialize_int(geomfile, sf->tippecanoe_maxzoom, geompos, fname);
	}
	if (sf->has_id) {
		serialize_ulong_long(geomfile, sf->id, geompos, fname);
	}

	serialize_int(geomfile, sf->segment, geompos, fname);

	write_geometry(sf->geometry, geompos, geomfile, fname, wx, wy);
	serialize_byte(geomfile, VT_END, geompos, fname);
	if (sf->index != 0) {
		serialize_ulong_long(geomfile, sf->index, geompos, fname);
	}
	if (sf->extent != 0) {
		serialize_long_long(geomfile, sf->extent, geompos, fname);
	}

	serialize_long_long(geomfile, sf->metapos, geompos, fname);

	if (sf->metapos < 0) {
		serialize_long_long(geomfile, sf->keys.size(), geompos, fname);

		for (size_t i = 0; i < sf->keys.size(); i++) {
			serialize_long_long(geomfile, sf->keys[i], geompos, fname);
			serialize_long_long(geomfile, sf->values[i], geompos, fname);
		}
	}

	if (include_minzoom) {
		serialize_byte(geomfile, sf->feature_minzoom, geompos, fname);
	}
}

serial_feature deserialize_feature(FILE *geoms, std::atomic<long long> *geompos_in, char *metabase, long long *meta_off, unsigned z, unsigned tx, unsigned ty, unsigned *initial_x, unsigned *initial_y) {
	serial_feature sf;

	deserialize_byte_io(geoms, &sf.t, geompos_in);
	if (sf.t < 0) {
		return sf;
	}

	deserialize_long_long_io(geoms, &sf.layer, geompos_in);

	sf.seq = 0;
	if (sf.layer & (1 << 5)) {
		deserialize_long_long_io(geoms, &sf.seq, geompos_in);
	}

	sf.tippecanoe_minzoom = -1;
	sf.tippecanoe_maxzoom = -1;
	sf.id = 0;
	sf.has_id = false;
	if (sf.layer & (1 << 1)) {
		deserialize_int_io(geoms, &sf.tippecanoe_minzoom, geompos_in);
	}
	if (sf.layer & (1 << 0)) {
		deserialize_int_io(geoms, &sf.tippecanoe_maxzoom, geompos_in);
	}
	if (sf.layer & (1 << 2)) {
		sf.has_id = true;
		deserialize_ulong_long_io(geoms, &sf.id, geompos_in);
	}

	deserialize_int_io(geoms, &sf.segment, geompos_in);

	sf.index = 0;
	sf.extent = 0;

	sf.geometry = decode_geometry(geoms, geompos_in, z, tx, ty, sf.bbox, initial_x[sf.segment], initial_y[sf.segment]);
	if (sf.layer & (1 << 4)) {
		deserialize_ulong_long_io(geoms, &sf.index, geompos_in);
	}
	if (sf.layer & (1 << 3)) {
		deserialize_long_long_io(geoms, &sf.extent, geompos_in);
	}

	sf.layer >>= 6;

	sf.metapos = 0;
	deserialize_long_long_io(geoms, &sf.metapos, geompos_in);

	if (sf.metapos >= 0) {
		char *meta = metabase + sf.metapos + meta_off[sf.segment];
		long long count;
		deserialize_long_long(&meta, &count);

		for (long long i = 0; i < count; i++) {
			long long k, v;
			deserialize_long_long(&meta, &k);
			deserialize_long_long(&meta, &v);
			sf.keys.push_back(k);
			sf.values.push_back(v);
		}
	} else {
		long long count;
		deserialize_long_long_io(geoms, &count, geompos_in);

		for (long long i = 0; i < count; i++) {
			long long k, v;
			deserialize_long_long_io(geoms, &k, geompos_in);
			deserialize_long_long_io(geoms, &v, geompos_in);
			sf.keys.push_back(k);
			sf.values.push_back(v);
		}
	}

	deserialize_byte_io(geoms, &sf.feature_minzoom, geompos_in);

	return sf;
}

static long long scale_geometry(struct serialization_state *sst, long long *bbox, drawvec &geom) {
	long long offset = 0;
	long long prev = 0;
	bool has_prev = false;
	double scale = 1.0 / (1 << geometry_scale);

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO || geom[i].op == VT_LINETO) {
			long long x = geom[i].x;
			long long y = geom[i].y;

			if (additional[A_DETECT_WRAPAROUND]) {
				x += offset;
				if (has_prev) {
					if (x - prev > (1LL << 31)) {
						offset -= 1LL << 32;
						x -= 1LL << 32;
					} else if (prev - x > (1LL << 31)) {
						offset += 1LL << 32;
						x += 1LL << 32;
					}
				}

				has_prev = true;
				prev = x;
			}

			if (x < bbox[0]) {
				bbox[0] = x;
			}
			if (y < bbox[1]) {
				bbox[1] = y;
			}
			if (x > bbox[2]) {
				bbox[2] = x;
			}
			if (y > bbox[3]) {
				bbox[3] = y;
			}

			if (!*(sst->initialized)) {
				if (x < 0 || x >= (1LL << 32) || y < 0 || y >= (1LL << 32)) {
					*(sst->initial_x) = 1LL << 31;
					*(sst->initial_y) = 1LL << 31;
				} else {
					*(sst->initial_x) = (((x + COORD_OFFSET) >> geometry_scale) << geometry_scale) - COORD_OFFSET;
					*(sst->initial_y) = (((y + COORD_OFFSET) >> geometry_scale) << geometry_scale) - COORD_OFFSET;
				}

				*(sst->initialized) = 1;
			}

			if (additional[A_GRID_LOW_ZOOMS]) {
				// If we are gridding, snap to the maxzoom grid in case the incoming data
				// is already supposed to be aligned to tile boundaries (but is not, exactly,
				// because of rounding error during projection).

				geom[i].x = std::round(x * scale);
				geom[i].y = std::round(y * scale);
			} else {
				geom[i].x = SHIFT_RIGHT(x);
				geom[i].y = SHIFT_RIGHT(y);
			}
		}
	}

	return geom.size();
}

static std::string strip_zeroes(std::string s) {
	// Doesn't do anything special with '-' followed by leading zeros
	// since integer IDs must be positive

	while (s.size() > 0 && s[0] == '0') {
		s.erase(s.begin());
	}

	return s;
}

// called from frontends
int serialize_feature(struct serialization_state *sst, serial_feature &sf) {
	struct reader *r = &(*sst->readers)[sst->segment];

	sf.bbox[0] = LLONG_MAX;
	sf.bbox[1] = LLONG_MAX;
	sf.bbox[2] = LLONG_MIN;
	sf.bbox[3] = LLONG_MIN;
	scale_geometry(sst, sf.bbox, sf.geometry);

	// This has to happen after scaling so that the wraparound detection has happened first.
	// Otherwise the inner/outer calculation will be confused by bad geometries.
	if (sf.t == VT_POLYGON) {
		sf.geometry = fix_polygon(sf.geometry);
	}

	for (auto &c : clipbboxes) {
		if (sf.t == VT_POLYGON) {
			sf.geometry = simple_clip_poly(sf.geometry, SHIFT_RIGHT(c.minx), SHIFT_RIGHT(c.miny), SHIFT_RIGHT(c.maxx), SHIFT_RIGHT(c.maxy));
		} else if (sf.t == VT_LINE) {
			sf.geometry = clip_lines(sf.geometry, SHIFT_RIGHT(c.minx), SHIFT_RIGHT(c.miny), SHIFT_RIGHT(c.maxx), SHIFT_RIGHT(c.maxy));
			sf.geometry = remove_noop(sf.geometry, sf.t, 0);
		} else if (sf.t == VT_POINT) {
			sf.geometry = clip_point(sf.geometry, SHIFT_RIGHT(c.minx), SHIFT_RIGHT(c.miny), SHIFT_RIGHT(c.maxx), SHIFT_RIGHT(c.maxy));
		}

		sf.bbox[0] = LLONG_MAX;
		sf.bbox[1] = LLONG_MAX;
		sf.bbox[2] = LLONG_MIN;
		sf.bbox[3] = LLONG_MIN;

		for (auto &g : sf.geometry) {
			long long x = SHIFT_LEFT(g.x);
			long long y = SHIFT_LEFT(g.y);

			if (x < sf.bbox[0]) {
				sf.bbox[0] = x;
			}
			if (y < sf.bbox[1]) {
				sf.bbox[1] = y;
			}
			if (x > sf.bbox[2]) {
				sf.bbox[2] = x;
			}
			if (y > sf.bbox[3]) {
				sf.bbox[3] = y;
			}
		}
	}

	if (sf.geometry.size() == 0) {
		// Feature was clipped away
		return 1;
	}

	if (!sf.has_id) {
		if (additional[A_GENERATE_IDS]) {
			sf.has_id = true;
			sf.id = sf.seq + 1;
		}
	}

	if (sst->want_dist) {
		std::vector<unsigned long long> locs;
		for (size_t i = 0; i < sf.geometry.size(); i++) {
			if (sf.geometry[i].op == VT_MOVETO || sf.geometry[i].op == VT_LINETO) {
				locs.push_back(encode_index(SHIFT_LEFT(sf.geometry[i].x), SHIFT_LEFT(sf.geometry[i].y)));
			}
		}
		std::sort(locs.begin(), locs.end());
		size_t n = 0;
		double sum = 0;
		for (size_t i = 1; i < locs.size(); i++) {
			if (locs[i - 1] != locs[i]) {
				sum += log(locs[i] - locs[i - 1]);
				n++;
			}
		}
		if (n > 0) {
			double avg = exp(sum / n);
			// Convert approximately from tile units to feet
			double dist_ft = sqrt(avg) / 33;

			*(sst->dist_sum) += log(dist_ft) * n;
			*(sst->dist_count) += n;
		}
		locs.clear();
	}

	bool inline_meta = true;
	// Don't inline metadata for features that will span several tiles at maxzoom
	if (sf.geometry.size() > 0 && (sf.bbox[2] < sf.bbox[0] || sf.bbox[3] < sf.bbox[1])) {
		fprintf(stderr, "Internal error: impossible feature bounding box %llx,%llx,%llx,%llx\n", sf.bbox[0], sf.bbox[1], sf.bbox[2], sf.bbox[3]);
	}
	if (sf.bbox[0] == LLONG_MAX) {
		// No bounding box (empty geometry)
		// Shouldn't happen, but avoid arithmetic overflow below
	} else if (sf.bbox[2] - sf.bbox[0] > (2LL << (32 - sst->maxzoom)) || sf.bbox[3] - sf.bbox[1] > (2LL << (32 - sst->maxzoom))) {
		inline_meta = false;

		if (prevent[P_CLIPPING]) {
			static std::atomic<long long> warned(0);
			long long extent = ((sf.bbox[2] - sf.bbox[0]) / ((1LL << (32 - sst->maxzoom)) + 1)) * ((sf.bbox[3] - sf.bbox[1]) / ((1LL << (32 - sst->maxzoom)) + 1));
			if (extent > warned) {
				fprintf(stderr, "Warning: %s:%d: Large unclipped (-pc) feature may be duplicated across %lld tiles\n", sst->fname, sst->line, extent);
				warned = extent;

				if (extent > 10000) {
					fprintf(stderr, "Exiting because this can't be right.\n");
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	double extent = 0;
	if (additional[A_DROP_SMALLEST_AS_NEEDED] || additional[A_COALESCE_SMALLEST_AS_NEEDED]) {
		if (sf.t == VT_POLYGON) {
			for (size_t i = 0; i < sf.geometry.size(); i++) {
				if (sf.geometry[i].op == VT_MOVETO) {
					size_t j;
					for (j = i + 1; j < sf.geometry.size(); j++) {
						if (sf.geometry[j].op != VT_LINETO) {
							break;
						}
					}

					extent += get_area(sf.geometry, i, j);
					i = j - 1;
				}
			}
		} else if (sf.t == VT_LINE) {
			for (size_t i = 1; i < sf.geometry.size(); i++) {
				if (sf.geometry[i].op == VT_LINETO) {
					double xd = sf.geometry[i].x - sf.geometry[i - 1].x;
					double yd = sf.geometry[i].y - sf.geometry[i - 1].y;
					extent += sqrt(xd * xd + yd * yd);
				}
			}
		}
	}

	if (extent <= LLONG_MAX) {
		sf.extent = (long long) extent;
	} else {
		sf.extent = LLONG_MAX;
	}

	if (!prevent[P_INPUT_ORDER]) {
		sf.seq = 0;
	}

	long long bbox_index;

	// Calculate the center even if off the edge of the plane,
	// and then mask to bring it back into the addressable area
	long long midx = (sf.bbox[0] / 2 + sf.bbox[2] / 2) & ((1LL << 32) - 1);
	long long midy = (sf.bbox[1] / 2 + sf.bbox[3] / 2) & ((1LL << 32) - 1);
	bbox_index = encode_index(midx, midy);

	if (additional[A_DROP_DENSEST_AS_NEEDED] || additional[A_COALESCE_DENSEST_AS_NEEDED] || additional[A_CLUSTER_DENSEST_AS_NEEDED] || additional[A_CALCULATE_FEATURE_DENSITY] || additional[A_INCREASE_GAMMA_AS_NEEDED] || sst->uses_gamma || cluster_distance != 0) {
		sf.index = bbox_index;
	} else {
		sf.index = 0;
	}

	if (sst->layermap->count(sf.layername) == 0) {
		sst->layermap->insert(std::pair<std::string, layermap_entry>(sf.layername, layermap_entry(sst->layermap->size())));
	}

	auto ai = sst->layermap->find(sf.layername);
	if (ai != sst->layermap->end()) {
		sf.layer = ai->second.id;

		if (!sst->filters) {
			if (sf.t == VT_POINT) {
				ai->second.points++;
			} else if (sf.t == VT_LINE) {
				ai->second.lines++;
			} else if (sf.t == VT_POLYGON) {
				ai->second.polygons++;
			}
		}
	} else {
		fprintf(stderr, "Internal error: can't find layer name %s\n", sf.layername.c_str());
		exit(EXIT_FAILURE);
	}

	for (ssize_t i = (ssize_t) sf.full_keys.size() - 1; i >= 0; i--) {
		coerce_value(sf.full_keys[i], sf.full_values[i].type, sf.full_values[i].s, sst->attribute_types);

		if (sf.full_keys[i] == attribute_for_id) {
			if (sf.full_values[i].type != mvt_double && !additional[A_CONVERT_NUMERIC_IDS]) {
				static bool warned = false;

				if (!warned) {
					fprintf(stderr, "Warning: Attribute \"%s\"=\"%s\" as feature ID is not a number\n", sf.full_keys[i].c_str(), sf.full_values[i].s.c_str());
					warned = true;
				}
			} else {
				char *err;
				long long id_value = strtoull(sf.full_values[i].s.c_str(), &err, 10);

				if (err != NULL && *err != '\0') {
					static bool warned_frac = false;

					if (!warned_frac) {
						fprintf(stderr, "Warning: Can't represent non-integer feature ID %s\n", sf.full_values[i].s.c_str());
						warned_frac = true;
					}
				} else if (std::to_string(id_value) != strip_zeroes(sf.full_values[i].s)) {
					static bool warned = false;

					if (!warned) {
						fprintf(stderr, "Warning: Can't represent too-large feature ID %s\n", sf.full_values[i].s.c_str());
						warned = true;
					}
				} else {
					sf.id = id_value;
					sf.has_id = true;

					sf.full_keys.erase(sf.full_keys.begin() + i);
					sf.full_values.erase(sf.full_values.begin() + i);
					continue;
				}
			}
		}

		if (sst->exclude_all) {
			if (sst->include->count(sf.full_keys[i]) == 0) {
				sf.full_keys.erase(sf.full_keys.begin() + i);
				sf.full_values.erase(sf.full_values.begin() + i);
				continue;
			}
		} else if (sst->exclude->count(sf.full_keys[i]) != 0) {
			sf.full_keys.erase(sf.full_keys.begin() + i);
			sf.full_values.erase(sf.full_values.begin() + i);
			continue;
		}
	}

	if (!sst->filters) {
		for (size_t i = 0; i < sf.full_keys.size(); i++) {
			type_and_string attrib;
			attrib.type = sf.full_values[i].type;
			attrib.string = sf.full_values[i].s;

			auto fk = sst->layermap->find(sf.layername);
			add_to_file_keys(fk->second.file_keys, sf.full_keys[i], attrib);
		}
	}

	if (inline_meta) {
		sf.metapos = -1;
		for (size_t i = 0; i < sf.full_keys.size(); i++) {
			sf.keys.push_back(addpool(r->poolfile, r->treefile, sf.full_keys[i].c_str(), mvt_string));
			sf.values.push_back(addpool(r->poolfile, r->treefile, sf.full_values[i].s.c_str(), sf.full_values[i].type));
		}
	} else {
		sf.metapos = r->metapos;
		serialize_long_long(r->metafile, sf.full_keys.size(), &r->metapos, sst->fname);
		for (size_t i = 0; i < sf.full_keys.size(); i++) {
			serialize_long_long(r->metafile, addpool(r->poolfile, r->treefile, sf.full_keys[i].c_str(), mvt_string), &r->metapos, sst->fname);
			serialize_long_long(r->metafile, addpool(r->poolfile, r->treefile, sf.full_values[i].s.c_str(), sf.full_values[i].type), &r->metapos, sst->fname);
		}
	}

	long long geomstart = r->geompos;
	serialize_feature(r->geomfile, &sf, &r->geompos, sst->fname, SHIFT_RIGHT(*(sst->initial_x)), SHIFT_RIGHT(*(sst->initial_y)), false);

	struct index index;
	index.start = geomstart;
	index.end = r->geompos;
	index.segment = sst->segment;
	index.seq = *(sst->layer_seq);
	index.t = sf.t;
	index.ix = bbox_index;

	fwrite_check(&index, sizeof(struct index), 1, r->indexfile, sst->fname);
	r->indexpos += sizeof(struct index);

	for (size_t i = 0; i < 2; i++) {
		if (sf.bbox[i] < r->file_bbox[i]) {
			r->file_bbox[i] = sf.bbox[i];
		}
	}
	for (size_t i = 2; i < 4; i++) {
		if (sf.bbox[i] > r->file_bbox[i]) {
			r->file_bbox[i] = sf.bbox[i];
		}
	}

	if (*(sst->progress_seq) % 10000 == 0) {
		checkdisk(sst->readers);
		if (!quiet && !quiet_progress && progress_time()) {
			fprintf(stderr, "Read %.2f million features\r", *sst->progress_seq / 1000000.0);
		}
	}
	(*(sst->progress_seq))++;
	(*(sst->layer_seq))++;

	return 1;
}

void coerce_value(std::string const &key, int &vt, std::string &val, std::map<std::string, int> const *attribute_types) {
	auto a = (*attribute_types).find(key);
	if (a != attribute_types->end()) {
		if (a->second == mvt_string) {
			vt = mvt_string;
		} else if (a->second == mvt_float) {
			vt = mvt_double;
			val = milo::dtoa_milo(atof(val.c_str()));
		} else if (a->second == mvt_int) {
			vt = mvt_double;
			if (val.size() == 0) {
				val = "0";
			}

			for (size_t ii = 0; ii < val.size(); ii++) {
				char c = val[ii];
				if (c < '0' || c > '9') {
					val = std::to_string(round(atof(val.c_str())));
					break;
				}
			}
		} else if (a->second == mvt_bool) {
			if (val == "false" || val == "0" || val == "null" || val.size() == 0 || (vt == mvt_double && atof(val.c_str()) == 0)) {
				vt = mvt_bool;
				val = "false";
			} else {
				vt = mvt_bool;
				val = "true";
			}
		} else {
			fprintf(stderr, "Can't happen: attribute type %d\n", a->second);
			exit(EXIT_FAILURE);
		}
	}
}
