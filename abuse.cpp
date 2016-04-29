#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <set>
#include <string>
#include <sqlite3.h>
#include <time.h>
#include <sys/time.h>
#include "jsonpull/jsonpull.h"
#include "mbtiles.hpp"
#include "main.hpp"
#include "geometry.hpp"
#include "geojson.hpp"

int geometry_scale = 1;
int CPUS = 1;
int TEMP_FILES = 1;
int quiet = 0;

void checkdisk(reader*, int) {

}

bool same(drawvec &one, drawvec &two) {
	if (one.size() != two.size()) {
		fprintf(stderr, "%ld != %ld\n", one.size(), two.size());
		return false;
	}

	for (size_t i = 0; i < one.size(); i++) {
		if (one[i].x != two[i].x || one[i].y != two[i].y) {
			fprintf(stderr, "%lld != %lld || %lld != %lld\n", one[i].x, two[i].x, one[i].y, two[i].y);
			return false;
		}
	}

	return true;
}

int main() {
	const bool loop = false;

	while (1) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		int seed = tv.tv_sec ^ tv.tv_usec;
		srand(seed);
		fprintf(stderr, "srand(%d)\n", seed);

		int sides = rand() % 50 + 3;
		drawvec dv;
		for (int i = 0; i < sides; i++) {
			int x = rand() % 20;
			int y = rand() % 20;

			if (i == 0) {
				dv.push_back(draw(VT_MOVETO, x, y));
			} else {
				dv.push_back(draw(VT_LINETO, x, y));
			}
		}
		dv.push_back(draw(VT_LINETO, dv[0].x, dv[0].y));

		drawvec orig = dv;
		drawvec out;

		bool again = true;
		while (again) {
			again = false;
			out = clean_or_clip_poly(dv, 0, 0, 0, false);

			if (loop) {
				if (!same(out, dv)) {
					fprintf(stderr, "loop\n");
					dv = out;
					again = true;
				}
			}
		}

		check_polygon(out, orig);
	}
}
