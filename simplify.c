#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include "projection.h"

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
			fprintf(stderr, "Couldn't understand %s", s);
			continue;
		}

		if (seq++ % 10000 == 0) {
			fprintf(stderr, "Read %.2f million points\r", seq / 1000000.0);
		}

		unsigned x, y;
		latlon2tile(lat, lon, 32, &x, &y);

		unsigned long long enc = encode(x, y);
		fwrite(&enc, 1, sizeof(unsigned long long), fp);
		size += sizeof(unsigned long long);
	}

	fclose(fp);

	unsigned long long *geom = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (geom == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}
}
