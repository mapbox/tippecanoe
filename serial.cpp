#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <set>
#include <map>
#include "protozero/varint.hpp"
#include "geometry.hpp"
#include "mbtiles.hpp"
#include "tile.hpp"
#include "serial.hpp"

size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname) {
	size_t w = fwrite(ptr, size, nitems, stream);
	if (w != nitems) {
		fprintf(stderr, "%s: Write to temporary file failed: %s\n", fname, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return w;
}

void serialize_int(FILE *out, int n, long long *fpos, const char *fname) {
	serialize_long_long(out, n, fpos, fname);
}

void serialize_long_long(FILE *out, long long n, long long *fpos, const char *fname) {
	unsigned long long zigzag = protozero::encode_zigzag64(n);

	serialize_ulong_long(out, zigzag, fpos, fname);
}

void serialize_ulong_long(FILE *out, unsigned long long zigzag, long long *fpos, const char *fname) {
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

void serialize_byte(FILE *out, signed char n, long long *fpos, const char *fname) {
	fwrite_check(&n, sizeof(signed char), 1, out, fname);
	*fpos += sizeof(signed char);
}

void serialize_uint(FILE *out, unsigned n, long long *fpos, const char *fname) {
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

int deserialize_long_long_io(FILE *f, long long *n, long long *geompos) {
	unsigned long long zigzag = 0;
	int ret = deserialize_ulong_long_io(f, &zigzag, geompos);
	*n = protozero::decode_zigzag64(zigzag);
	return ret;
}

int deserialize_ulong_long_io(FILE *f, unsigned long long *zigzag, long long *geompos) {
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

int deserialize_int_io(FILE *f, int *n, long long *geompos) {
	long long ll = 0;
	int ret = deserialize_long_long_io(f, &ll, geompos);
	*n = ll;
	return ret;
}

int deserialize_uint_io(FILE *f, unsigned *n, long long *geompos) {
	if (fread(n, sizeof(unsigned), 1, f) != 1) {
		return 0;
	}
	*geompos += sizeof(unsigned);
	return 1;
}

int deserialize_byte_io(FILE *f, signed char *n, long long *geompos) {
	int c = getc(f);
	if (c == EOF) {
		return 0;
	}
	*n = c;
	(*geompos)++;
	return 1;
}

static void write_geometry(drawvec const &dv, long long *fpos, FILE *out, const char *fname, long long wx, long long wy) {
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

void serialize_feature(FILE *geomfile, serial_feature *sf, long long *geompos, const char *fname, long long wx, long long wy, bool include_minzoom) {
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

	serialize_int(geomfile, sf->m, geompos, fname);
	if (sf->m != 0) {
		serialize_long_long(geomfile, sf->metapos, geompos, fname);
	}

	if (sf->metapos < 0 && sf->m != sf->keys.size()) {
		fprintf(stderr, "Internal error: %lld doesn't match %lld\n", (long long) sf->m, (long long) sf->keys.size());
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < sf->keys.size(); i++) {
		serialize_long_long(geomfile, sf->keys[i], geompos, fname);
		serialize_long_long(geomfile, sf->values[i], geompos, fname);
	}

	if (include_minzoom) {
		serialize_byte(geomfile, sf->feature_minzoom, geompos, fname);
	}
}
