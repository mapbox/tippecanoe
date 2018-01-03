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

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname) {
	size_t w = fwrite(ptr, size, nitems, stream);
	if (w != nitems) {
		fprintf(stderr, "%s: Write to temporary file failed: %s\n", fname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return w;
}

void serialize_int(FILE *out, int n, size_t *fpos, const char *fname) {
	serialize_long(out, n, fpos, fname);
}

void serialize_long(FILE *out, long n, size_t *fpos, const char *fname) {
	unsigned long zigzag = protozero::encode_zigzag64(n);

	serialize_ulong(out, zigzag, fpos, fname);
}

void serialize_ulong(FILE *out, unsigned long zigzag, size_t *fpos, const char *fname) {
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

void serialize_byte(FILE *out, signed char n, size_t *fpos, const char *fname) {
	fwrite_check(&n, sizeof(signed char), 1, out, fname);
	*fpos += sizeof(signed char);
}

void serialize_ubyte(FILE *out, unsigned char n, size_t *fpos, const char *fname) {
	fwrite_check(&n, sizeof(unsigned char), 1, out, fname);
	*fpos += sizeof(unsigned char);
}

void serialize_uint(FILE *out, unsigned n, size_t *fpos, const char *fname) {
	fwrite_check(&n, sizeof(unsigned), 1, out, fname);
	*fpos += sizeof(unsigned);
}

void deserialize_int(char **f, int *n) {
	long ll;
	deserialize_long(f, &ll);
	*n = (int) ll;
}

void deserialize_long(char **f, long *n) {
	unsigned long zigzag = 0;
	deserialize_ulong(f, &zigzag);
	*n = protozero::decode_zigzag64(zigzag);
}

void deserialize_ulong(char **f, unsigned long *zigzag) {
	*zigzag = 0;
	size_t shift = 0;

	while (1) {
		if ((**f & 0x80) == 0) {
			*zigzag |= ((unsigned long) **f) << shift;
			*f += 1;
			shift += 7;
			break;
		} else {
			*zigzag |= ((unsigned long) (**f & 0x7F)) << shift;
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

void deserialize_ubyte(char **f, unsigned char *n) {
	memcpy(n, *f, sizeof(unsigned char));
	*f += sizeof(unsigned char);
}

bool deserialize_long_io(FILE *f, long *n, size_t *geompos) {
	unsigned long zigzag = 0;
	bool ret = deserialize_ulong_io(f, &zigzag, geompos);
	*n = protozero::decode_zigzag64(zigzag);
	return ret;
}

bool deserialize_ulong_io(FILE *f, unsigned long *zigzag, size_t *geompos) {
	*zigzag = 0;
	size_t shift = 0;

	while (1) {
		int c = getc(f);
		if (c == EOF) {
			return 0;
		}
		(*geompos)++;

		if ((c & 0x80) == 0) {
			*zigzag |= ((unsigned long) c) << shift;
			shift += 7;
			break;
		} else {
			*zigzag |= ((unsigned long) (c & 0x7F)) << shift;
			shift += 7;
		}
	}

	return 1;
}

bool deserialize_int_io(FILE *f, int *n, size_t *geompos) {
	long ll = 0;
	bool ret = deserialize_long_io(f, &ll, geompos);
	*n = (int) ll;
	return ret;
}

bool deserialize_uint_io(FILE *f, unsigned *n, size_t *geompos) {
	if (fread(n, sizeof(unsigned), 1, f) != 1) {
		return 0;
	}
	*geompos += sizeof(unsigned);
	return 1;
}

bool deserialize_byte_io(FILE *f, signed char *n, size_t *geompos) {
	int c = getc(f);
	if (c == EOF) {
		return 0;
	}
	*n = (signed char) c;
	(*geompos)++;
	return 1;
}

bool deserialize_ubyte_io(FILE *f, unsigned char *n, size_t *geompos) {
	int c = getc(f);
	if (c == EOF) {
		return 0;
	}
	*n = (unsigned char) c;
	(*geompos)++;
	return 1;
}

static void write_geometry(drawvec const &dv, size_t *fpos, FILE *out, const char *fname, long wx, long wy) {
	for (size_t i = 0; i < dv.size(); i++) {
		if (dv[i].op == VT_MOVETO || dv[i].op == VT_LINETO) {
			serialize_ubyte(out, dv[i].op, fpos, fname);
			serialize_long(out, dv[i].x - wx, fpos, fname);
			serialize_long(out, dv[i].y - wy, fpos, fname);
			wx = dv[i].x;
			wy = dv[i].y;
		} else {
			serialize_ubyte(out, dv[i].op, fpos, fname);
		}
	}
}

void serialize_feature(FILE *geomfile, serial_feature *sf, size_t *geompos, const char *fname, long wx, long wy, bool include_minzoom) {
	serialize_byte(geomfile, sf->t, geompos, fname);

	size_t layer = 0;
	layer |= sf->layer << 7;
	layer |= (sf->has_metapos ? 1U : 0U) << 6;
	layer |= (sf->seq != 0 ? 1U : 0U) << 5;
	layer |= (sf->index != 0 ? 1U : 0U) << 4;
	layer |= (sf->extent != 0 ? 1U : 0U) << 3;
	layer |= (sf->has_id ? 1U : 0U) << 2;
	layer |= (sf->has_tippecanoe_minzoom ? 1U : 0U) << 1;
	layer |= (sf->has_tippecanoe_maxzoom ? 1U : 0U) << 0;

	serialize_ulong(geomfile, layer, geompos, fname);
	if (sf->seq != 0) {
		serialize_ulong(geomfile, sf->seq, geompos, fname);
	}
	if (sf->has_tippecanoe_minzoom) {
		serialize_int(geomfile, sf->tippecanoe_minzoom, geompos, fname);
	}
	if (sf->has_tippecanoe_maxzoom) {
		serialize_int(geomfile, sf->tippecanoe_maxzoom, geompos, fname);
	}
	if (sf->has_id) {
		serialize_ulong(geomfile, sf->id, geompos, fname);
	}

	serialize_ulong(geomfile, sf->segment, geompos, fname);

	write_geometry(sf->geometry, geompos, geomfile, fname, wx, wy);
	serialize_byte(geomfile, VT_END, geompos, fname);
	if (sf->index != 0) {
		serialize_ulong(geomfile, sf->index, geompos, fname);
	}
	if (sf->extent != 0) {
		serialize_long(geomfile, sf->extent, geompos, fname);
	}

	serialize_ulong(geomfile, sf->m, geompos, fname);
	if (sf->m != 0) {
		serialize_ulong(geomfile, sf->metapos, geompos, fname);
	}

	if (!sf->has_metapos && sf->m != sf->keys.size()) {
		fprintf(stderr, "Internal error: feature said to have %ld attributes but only %ld found\n", (long) sf->m, (long) sf->keys.size());
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < sf->keys.size(); i++) {
		serialize_ulong(geomfile, sf->keys[i], geompos, fname);
		serialize_ulong(geomfile, sf->values[i], geompos, fname);
	}

	if (include_minzoom) {
		serialize_byte(geomfile, sf->feature_minzoom, geompos, fname);
	}
}

serial_feature deserialize_feature(FILE *geoms, size_t *geompos_in, char *metabase, size_t *meta_off, int z, unsigned tx, unsigned ty, long *initial_x, long *initial_y) {
	serial_feature sf;

	deserialize_byte_io(geoms, &sf.t, geompos_in);
	if (sf.t < 0) {
		return sf;
	}

	deserialize_ulong_io(geoms, &sf.layer, geompos_in);

	sf.seq = 0;
	if (sf.layer & (1 << 5)) {
		deserialize_ulong_io(geoms, &sf.seq, geompos_in);
	}

	sf.tippecanoe_minzoom = -1;
	sf.tippecanoe_maxzoom = -1;
	sf.id = 0;
	sf.has_id = false;
	sf.metapos = false;
	if (sf.layer & (1 << 1)) {
		deserialize_int_io(geoms, &sf.tippecanoe_minzoom, geompos_in);
	}
	if (sf.layer & (1 << 0)) {
		deserialize_int_io(geoms, &sf.tippecanoe_maxzoom, geompos_in);
	}
	if (sf.layer & (1 << 2)) {
		sf.has_id = true;
		deserialize_ulong_io(geoms, &sf.id, geompos_in);
	}
	if (sf.layer & (1 << 6)) {
		sf.has_metapos = true;
	}

	deserialize_ulong_io(geoms, &sf.segment, geompos_in);

	sf.index = 0;
	sf.extent = 0;

	sf.geometry = decode_geometry(geoms, geompos_in, z, tx, ty, sf.bbox, initial_x[sf.segment], initial_y[sf.segment]);
	if (sf.layer & (1 << 4)) {
		deserialize_ulong_io(geoms, &sf.index, geompos_in);
	}
	if (sf.layer & (1 << 3)) {
		deserialize_long_io(geoms, &sf.extent, geompos_in);
	}

	sf.layer >>= 7;

	sf.metapos = 0;
	{
		size_t m;
		deserialize_ulong_io(geoms, &m, geompos_in);
		sf.m = m;
	}
	if (sf.m != 0) {
		size_t metapos;
		deserialize_ulong_io(geoms, &metapos, geompos_in);
		sf.metapos = metapos;
	}

	if (sf.has_metapos) {
		char *meta = metabase + sf.metapos + meta_off[sf.segment];

		for (size_t i = 0; i < sf.m; i++) {
			size_t k, v;
			deserialize_ulong(&meta, &k);
			deserialize_ulong(&meta, &v);
			sf.keys.push_back(k);
			sf.values.push_back(v);
		}
	} else {
		for (size_t i = 0; i < sf.m; i++) {
			size_t k, v;
			deserialize_ulong_io(geoms, &k, geompos_in);
			deserialize_ulong_io(geoms, &v, geompos_in);
			sf.keys.push_back(k);
			sf.values.push_back(v);
		}
	}

	deserialize_byte_io(geoms, &sf.feature_minzoom, geompos_in);

	return sf;
}

static size_t scale_geometry(struct serialization_state *sst, long *bbox, drawvec &geom) {
	long offset = 0;
	long prev = 0;
	bool has_prev = false;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == VT_MOVETO || geom[i].op == VT_LINETO) {
			long x = geom[i].x;
			long y = geom[i].y;

			if (additional[A_DETECT_WRAPAROUND]) {
				x += offset;
				if (has_prev) {
					if (x - prev > (1L << 31)) {
						offset -= 1L << 32;
						x -= 1L << 32;
					} else if (prev - x > (1L << 31)) {
						offset += 1L << 32;
						x += 1L << 32;
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
				if (x < 0 || x >= (1L << 32) || y < 0 || y >= (1L < 32)) {
					*(sst->initial_x) = 1L << 31;
					*(sst->initial_y) = 1L << 31;
				} else {
					*(sst->initial_x) = (x >> geometry_scale) << geometry_scale;
					*(sst->initial_y) = (y >> geometry_scale) << geometry_scale;
				}

				*(sst->initialized) = 1;
			}

			geom[i].x = x >> geometry_scale;
			geom[i].y = y >> geometry_scale;
		}
	}

	return geom.size();
}

bool serialize_feature(struct serialization_state *sst, serial_feature &sf) {
	struct reader *r = &(*sst->readers)[sst->segment];

	sf.bbox[0] = LONG_MAX;
	sf.bbox[1] = LONG_MAX;
	sf.bbox[2] = LONG_MIN;
	sf.bbox[3] = LONG_MIN;
	scale_geometry(sst, sf.bbox, sf.geometry);

	// This has to happen after scaling so that the wraparound detection has happened first.
	// Otherwise the inner/outer calculation will be confused by bad geometries.
	if (sf.t == VT_POLYGON) {
		sf.geometry = fix_polygon(sf.geometry);
	}

	if (sst->want_dist) {
		std::vector<unsigned long> locs;
		for (size_t i = 0; i < sf.geometry.size(); i++) {
			if (sf.geometry[i].op == VT_MOVETO || sf.geometry[i].op == VT_LINETO) {
				long x = (sf.geometry[i].x << geometry_scale) & ((1L << 32) - 1);
				long y = (sf.geometry[i].y << geometry_scale) & ((1L << 32) - 1);
				// Out of bounds coordinates can be expected here in features
				// that cross the antimeridian. The mask will make small distances
				// look large. But the same thing also happens for features that cross
				// the prime meridian.
				locs.push_back(encode((unsigned) x, (unsigned) y));
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
		fprintf(stderr, "Internal error: impossible feature bounding box %lx,%lx,%lx,%lx\n", sf.bbox[0], sf.bbox[1], sf.bbox[2], sf.bbox[3]);
	}
	if (sf.bbox[0] == LONG_MAX) {
		// No bounding box (empty geometry)
		// Shouldn't happen, but avoid arithmetic overflow below
	} else if (sf.bbox[2] - sf.bbox[0] > (2L << (32 - sst->maxzoom)) || sf.bbox[3] - sf.bbox[1] > (2L << (32 - sst->maxzoom))) {
		inline_meta = false;

		if (prevent[P_CLIPPING]) {
			static volatile long warned = 0;
			long extent = ((sf.bbox[2] - sf.bbox[0]) / ((1L << (32 - sst->maxzoom)) + 1)) * ((sf.bbox[3] - sf.bbox[1]) / ((1L << (32 - sst->maxzoom)) + 1));
			if (extent > warned) {
				fprintf(stderr, "Warning: %s:%zu: Large unclipped (-pc) feature may be duplicated across %ld tiles\n", sst->fname, sst->line, extent);
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

	sf.extent = (long) extent;

	if (!prevent[P_INPUT_ORDER]) {
		sf.seq = 0;
	}

	unsigned long bbox_index;

	// Calculate the center even if off the edge of the plane,
	// and then mask to bring it back into the addressable area
	long midx = (sf.bbox[0] / 2 + sf.bbox[2] / 2) & ((1L << 32) - 1);
	long midy = (sf.bbox[1] / 2 + sf.bbox[3] / 2) & ((1L << 32) - 1);
	// The mask above brings coordinates into range, but means that
	// features that cross the antimeridian will have a surprising
	// quadkey.
	bbox_index = encode((unsigned) midx, (unsigned) midy);

	if (additional[A_DROP_DENSEST_AS_NEEDED] || additional[A_CLUSTER_DENSEST_AS_NEEDED] || additional[A_CALCULATE_FEATURE_DENSITY] || additional[A_INCREASE_GAMMA_AS_NEEDED] || sst->uses_gamma || cluster_distance != 0) {
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
		size_t ii = (size_t) i;

		if (sst->exclude_all) {
			if (sst->include->count(sf.full_keys[ii]) == 0) {
				sf.full_keys.erase(sf.full_keys.begin() + i);
				sf.full_values.erase(sf.full_values.begin() + i);
				sf.m--;
				continue;
			}
		} else if (sst->exclude->count(sf.full_keys[ii]) != 0) {
			sf.full_keys.erase(sf.full_keys.begin() + i);
			sf.full_values.erase(sf.full_values.begin() + i);
			sf.m--;
			continue;
		}

		coerce_value(sf.full_keys[ii], sf.full_values[ii].type, sf.full_values[ii].s, sst->attribute_types);
	}

	if (sst->filter != NULL) {
		std::map<std::string, mvt_value> attributes;

		for (size_t i = 0; i < sf.full_keys.size(); i++) {
			std::string key = sf.full_keys[i];
			mvt_value val = stringified_to_mvt_value(sf.full_values[i].type, sf.full_values[i].s.c_str());

			attributes.insert(std::pair<std::string, mvt_value>(key, val));
		}

		if (sf.has_id) {
			mvt_value v;
			v.type = mvt_uint;
			v.numeric_value.uint_value = sf.id;

			attributes.insert(std::pair<std::string, mvt_value>("$id", v));
		}

		mvt_value v;
		v.type = mvt_string;

		if (sf.t == mvt_point) {
			v.string_value = "Point";
		} else if (sf.t == mvt_linestring) {
			v.string_value = "LineString";
		} else if (sf.t == mvt_polygon) {
			v.string_value = "Polygon";
		}

		attributes.insert(std::pair<std::string, mvt_value>("$type", v));

		if (!evaluate(attributes, sf.layername, sst->filter)) {
			return false;
		}
	}

	for (ssize_t i = (ssize_t) sf.full_keys.size() - 1; i >= 0; i--) {
		if (sf.full_values[(size_t) i].type == mvt_null) {
			sf.full_keys.erase(sf.full_keys.begin() + i);
			sf.full_values.erase(sf.full_values.begin() + i);
			sf.m--;
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
		sf.has_metapos = false;
		sf.metapos = 0;
		for (size_t i = 0; i < sf.full_keys.size(); i++) {
			sf.keys.push_back(addpool(r->poolfile, r->treefile, sf.full_keys[i].c_str(), mvt_string));
			sf.values.push_back(addpool(r->poolfile, r->treefile, sf.full_values[i].s.c_str(), sf.full_values[i].type));
		}
	} else {
		sf.has_metapos = true;
		sf.metapos = r->metapos;
		for (size_t i = 0; i < sf.full_keys.size(); i++) {
			serialize_ulong(r->metafile, addpool(r->poolfile, r->treefile, sf.full_keys[i].c_str(), mvt_string), &r->metapos, sst->fname);
			serialize_ulong(r->metafile, addpool(r->poolfile, r->treefile, sf.full_values[i].s.c_str(), sf.full_values[i].type), &r->metapos, sst->fname);
		}
	}

	size_t geomstart = r->geompos;
	serialize_feature(r->geomfile, &sf, &r->geompos, sst->fname, *(sst->initial_x) >> geometry_scale, *(sst->initial_y) >> geometry_scale, false);

	if (sst->segment > USHRT_MAX) {
		fprintf(stderr, "Internal error: too many CPUs used to parse input: %zu\n", sst->segment);
		exit(EXIT_FAILURE);
	}

	struct index index;
	index.start = geomstart;
	index.end = r->geompos;
	index.segment = (unsigned short) sst->segment;
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
		if (!quiet && !quiet_progress) {
			fprintf(stderr, "Read %.2f million features\r", *sst->progress_seq / 1000000.0);
		}
	}
	(*(sst->progress_seq))++;
	(*(sst->layer_seq))++;

	return true;
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
