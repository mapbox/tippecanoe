#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>
#include <vector>

#include "tile.hpp"
#include "geometry.hpp"
#include "options.hpp"

int deserialize_long_long_io(FILE *f, long long *n, long long *geompos) {
	return 0;
}

int deserialize_int_io(FILE *f, int *n, long long *geompos) {
	return 0;
}

int deserialize_uint_io(FILE *f, unsigned *n, long long *geompos) {
	return 0;
}

int deserialize_byte_io(FILE *f, signed char *n, long long *geompos) {
	return 0;
}

int geometry_scale = 0;

int main() {
	srand(time(NULL));

	while (1) {
		drawvec dv;
		drawvec path;
		FILE *f = fopen("log", "w");

		int i;

		int len = rand() % 100 + 3;

		for (i = 0; i < len; i++) {
			int x = rand() % 100;
			int y = rand() % 100;
			fprintf(f, "%d,%d ", x, y);

			if (path.size() == 0) {
				path.push_back(draw(VT_MOVETO, x, y));
			} else {
				path.push_back(draw(VT_LINETO, x, y));
			}

			if (rand() % 50 == 0 && path.size() >= 3) {
				path.push_back(draw(VT_LINETO, path[0].x, path[0].y));
				for (size_t j = 0; j < path.size(); j++) {
					dv.push_back(path[j]);
				}
				path.clear();

				fprintf(f, "\n");
			}
		}

		if (path.size() >= 3) {
			path.push_back(draw(VT_LINETO, path[0].x, path[0].y));
			for (size_t j = 0; j < path.size(); j++) {
				dv.push_back(path[j]);
			}
			path.clear();

			fprintf(f, "\n");
		} else {
			fprintf(f, "<- unused\n");
		}

		fclose(f);

		printf("%d sides\n", len);
		clock_t before = clock();

		drawvec out = clean_or_clip_poly(dv, 0, 12, 0, false);

		clock_t after = clock();
		printf("%d sides took %lld\n", len, (long long) (after - before));

		check_polygon(out, dv);
	}
}
