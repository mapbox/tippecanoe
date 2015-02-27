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

double cubert(double x) {
	if (x == 0) {
		return 0;
	} else if (x < 0) {
		return -exp(log(-x) / 3);
	} else {
		return exp(log(x) / 3);
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
	double error = 0;
	double zerror = 0;
	long long prev = 0;

	for (i = 0; i < size - 1; i++) {
		if (geom[i + 1] == geom[i]) {
			long long j;
			for (j = i; j < size; j++) {
				if (geom[j] != geom[i]) {
					break;
				}
			}

			double want = cubert(j - i) + zerror;
			long long n = floor(want);
			zerror = want - n;

			long long m;
			for (m = 0; m < n; m++) {
				unsigned x, y;
				decode(geom[i], &x, &y);
				double lat, lon;
				tile2latlon(x, y, 32, &lat, &lon);
				printf("%.6f,%.6f // %lld from %lld\n", lat, lon, n, j - i);
			}

			i = j - 1;
		} else {
			if (prev == 0) {
				unsigned x, y;
				decode(geom[i], &x, &y);
				double lat, lon;
				tile2latlon(x, y, 32, &lat, &lon);
				printf("%.6f,%.6f // initial\n", lat, lon);
				prev = geom[i];
			} else {
				double want = 1.0 / cubert((geom[i + 1] - geom[i]) / (double) (1 << 20));
				double density = 1.0 / ((geom[i] - prev) / (double) (1 << 20));

				if (want + error >= density) {
					unsigned x, y;
					decode(geom[i], &x, &y);
					double lat, lon;
					tile2latlon(x, y, 32, &lat, &lon);
					printf("%.6f,%.6f // %f from %f\n", lat, lon, density, want);
					prev = geom[i];
					error = 0; // want + error - density;
				} else {
					unsigned x, y;
					decode(geom[i], &x, &y);
					double lat, lon;
					tile2latlon(x, y, 32, &lat, &lon);
					printf("skipping %.6f,%.6f // %f from %f error %f\n", lat, lon, density, want, error);

					error += want;
				}
			}
		}
	}
}
