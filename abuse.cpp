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

int main() {
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

		drawvec out = clean_or_clip_poly(dv, 0, 0, 0, false);
		check_polygon(out, dv);
	}
}
