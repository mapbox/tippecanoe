#include "clip.h"

#define INSIDE 0
#define LEFT 1
#define RIGHT 2
#define BOTTOM 4
#define TOP 8

static int computeOutCode(double x, double y, double xmin, double ymin, double xmax, double ymax) {
	int code = INSIDE;

	if (x < xmin) {
		code |= LEFT;
	} else if (x > xmax) {
		code |= RIGHT;
	}

	if (y < ymin) {
		code |= BOTTOM;
	} else if (y > ymax) {
		code |= TOP;
	}

	return code;
}

int clip(double *x0, double *y0, double *x1, double *y1, double xmin, double ymin, double xmax, double ymax) {
	int outcode0 = computeOutCode(*x0, *y0, xmin, ymin, xmax, ymax);
	int outcode1 = computeOutCode(*x1, *y1, xmin, ymin, xmax, ymax);
	int accept = 0;
	int changed = 0;
 
	while (1) {
		if (!(outcode0 | outcode1)) { // Bitwise OR is 0. Trivially accept and get out of loop
			accept = 1;
			break;
		} else if (outcode0 & outcode1) { // Bitwise AND is not 0. Trivially reject and get out of loop
			break;
		} else {
			// failed both tests, so calculate the line segment to clip
			// from an outside point to an intersection with clip edge
			double x = *x0, y = *y0;
 
			// At least one endpoint is outside the clip rectangle; pick it.
			int outcodeOut = outcode0 ? outcode0 : outcode1;
 
			// Now find the intersection point;
			// use formulas y = y0 + slope * (x - x0), x = x0 + (1 / slope) * (y - y0)
			if (outcodeOut & TOP) {           // point is above the clip rectangle
				x = *x0 + (*x1 - *x0) * (ymax - *y0) / (*y1 - *y0);
				y = ymax;
			} else if (outcodeOut & BOTTOM) { // point is below the clip rectangle
				x = *x0 + (*x1 - *x0) * (ymin - *y0) / (*y1 - *y0);
				y = ymin;
			} else if (outcodeOut & RIGHT) {  // point is to the right of clip rectangle
				y = *y0 + (*y1 - *y0) * (xmax - *x0) / (*x1 - *x0);
				x = xmax;
			} else if (outcodeOut & LEFT) {   // point is to the left of clip rectangle
				y = *y0 + (*y1 - *y0) * (xmin - *x0) / (*x1 - *x0);
				x = xmin;
			}
 
			// Now we move outside point to intersection point to clip
			// and get ready for next pass.
			if (outcodeOut == outcode0) {
				*x0 = x;
				*y0 = y;
				outcode0 = computeOutCode(*x0, *y0, xmin, ymin, xmax, ymax);
				changed = 1;
			} else {
				*x1 = x;
				*y1 = y;
				outcode1 = computeOutCode(*x1, *y1, xmin, ymin, xmax, ymax);
				changed = 1;
			}
		}
	}

	if (accept == 0) {
		return 0;
	} else {
		return changed + 1;
	}
}
