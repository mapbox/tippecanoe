#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/mman.h>
#include "projection.h"

int cmp(const void *v1, const void *v2) {
	const unsigned long long *p1 = v1;
	const unsigned long long *p2 = v2;

	if (*p1 < *p2) {
		return -1;
	} else if (*p1 > *p2) {
		return 1;
	} else {
		return 0;
	}
}

inline double fpow(double val, double e) {
	if (val == 0) {
		return 0;
	} else {
		return exp(log(val) * e);
	}
}

void out(unsigned long long index) {
	unsigned x, y;
	decode(index, &x, &y);

	double lat, lon;
	tile2latlon(x, y, 32, &lat, &lon);

	printf("%.6f,%.6f", lat, lon);
}

// in other words, a z14 pixel, about 25 feet
double scale = (double) (1LL << (64 - 2 * (14 + 8)));

void test() {
	srandomdev();
	int i;
	for (i = 0; i < 50000; i++) {
		unsigned long long r1 = random() << 32;
		r1 |= random();
		unsigned long long r2 = r1 + scale;

		unsigned x1, y1, x2, y2;
		decode(r1, &x1, &y1);
		decode(r2, &x2, &y2);

		double lat1, lon1, lat2, lon2;
		tile2latlon(x1, y1, 32, &lat1, &lon1);
		tile2latlon(x2, y2, 32, &lat2, &lon2);

		double rat = cos(lat1 + M_PI / 180);
		double latd = lat1 - lat2;
		double lond = (lon1 - lon2) * rat;
		double d = sqrt(latd * latd + lond * lond) / .00000274;

		if (d != 0) {
			printf("%.6f\n", d);
		}
	}
}

int main() {
	char s[2000];
	long long seq = 0;
	long long size = 0;

	char geomname[2000] = "/tmp/geom.XXXXXXXX";
	int fd = mkstemp(geomname);
	if (fd < 0) {
		perror(geomname);
		exit(EXIT_FAILURE);
	}
	FILE *fp = fopen(geomname, "wb");
	unlink(geomname);

	while (fgets(s, 2000, stdin)) {
		double lat, lon;
		if (sscanf(s, "%lf,%lf", &lat, &lon) != 2) {
			//fprintf(stderr, "Couldn't understand %s", s);
			continue;
		}

		if (seq++ % 10000 == 0) {
			fprintf(stderr, "Read %.2f million points\r", seq / 1000000.0);
		}

		unsigned x, y;
		latlon2tile(lat, lon, 32, &x, &y);

		unsigned long long enc = encode(x, y);
		fwrite(&enc, 1, sizeof(unsigned long long), fp);
		size++;
	}

	fclose(fp);

	fprintf(stderr, "sorting %lld points\n", size);

	unsigned long long *geom = mmap(NULL, size * sizeof(unsigned long long), PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (geom == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	qsort(geom, size, sizeof(unsigned long long), cmp);

	long long i;
	double exponent = 2.5;

	out(geom[0]);
	printf("\n");

	for (i = 1; i < size; i++) {
		long long dist = geom[i] - geom[i - 1];

		if (dist >= scale) {
			out(geom[i]);
			printf(" // far enough %lld vs %lf\n", dist, scale);
		} else if (dist == 0) {
			// eliminate exact duplicates.
			// or should we produce them, but fewer?
		} else {
			double gap = dist / scale;
			long long oi = i;
			long long count = 0;

			while (fpow((geom[i] - geom[oi - 1]) / scale, exponent) <= gap && i < size) {
				i++;
				count++;
			}

			if (i < size) {
				if ((i > oi) && (seq++ % 2 == 0)) {
					i--;
				}
				out(geom[i]);
				printf(" // skipped %lld\n", (i - oi));
			}
		}
	}
}
