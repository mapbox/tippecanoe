#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "protozero/varint.hpp"
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
	unsigned long long zigzag = protozero::encode_zigzag32(n);

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
	*n = protozero::decode_zigzag32(zigzag);
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
	*n = protozero::decode_zigzag32(zigzag);
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
