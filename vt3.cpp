#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <getopt.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <zlib.h>
#include <math.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <protozero/pbf_reader.hpp>
#include <sys/stat.h>
#include <algorithm>
#include "mvt.hpp"
#include "projection.hpp"
#include "geometry.hpp"
#include "vt3.hpp"
#include "clip.hpp"

std::vector<mvt_geometry> clip_lines(std::vector<mvt_geometry> &geom, long left, long top, long right, long bottom) {
	std::vector<mvt_geometry> out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (i > 0 && (geom[i - 1].op == mvt_moveto || geom[i - 1].op == mvt_lineto) && geom[i].op == mvt_lineto) {
			double x1 = geom[i - 1].x;
			double y1 = geom[i - 1].y;

			double x2 = geom[i - 0].x;
			double y2 = geom[i - 0].y;

			int c = clip(&x1, &y1, &x2, &y2, left, top, right, bottom);

			if (c == CLIP_ELIMINATED) {
				mvt_geometry outside(mvt_moveto, geom[i].x, geom[i].y);
				outside.phantom = true;
				outside.id = 0;
				out.push_back(outside);
			} else if (c == CLIP_OK) {
				out.push_back(geom[i]);
			} else if ((c & CLIP_CHANGED_FIRST) && (c & CLIP_CHANGED_SECOND)) {
#if 0
				fprintf(stderr, "Shouldn't happen in clipping\n");
				printf("%lld,%lld to %lld,%lld becomes %f,%f to %f,%f in %ld,%ld to %ld,%ld\n", geom[i - 1].x, geom[i - 1].y, geom[i].x, geom[i].y,
				       x1, y1, x2, y2, left, top, right, bottom);
				exit(EXIT_FAILURE);
#endif
				// These must both be on the edge of the buffer, outside the tile itself,
				// so both sides survive but as irrelevant phantom geometry

				mvt_geometry outside1(mvt_moveto, x1, y1);
				outside1.phantom = true;
				outside1.id = 0;

				mvt_geometry outside2(mvt_lineto, x2, y2);
				outside2.phantom = true;
				outside2.id = 0;

				out.push_back(outside1);
				out.push_back(outside2);

				// Do we really need this, or will the next segment always
				// start with a moveto?
				mvt_geometry noop(mvt_moveto, geom[i].x, geom[i].y);
				noop.phantom = true;
				noop.id = 0;
				out.push_back(noop);
			} else if (c & CLIP_CHANGED_FIRST) {
				mvt_geometry outside(mvt_moveto, x1, y1);
				outside.phantom = true;
				outside.id = 0;

				out.push_back(outside);
				out.push_back(geom[i]);
			} else if (c & CLIP_CHANGED_SECOND) {
				out.push_back(geom[i - 1]);

				mvt_geometry outside(mvt_lineto, x2, y2);
				outside.phantom = true;
				outside.id = 0;

				out.push_back(outside);

				// Do we really need this, or will the next segment always
				// start with a moveto?
				mvt_geometry noop(mvt_moveto, geom[i].x, geom[i].y);
				noop.phantom = true;
				noop.id = 0;
				out.push_back(noop);
			} else {
				fprintf(stderr, "Can't happen in clipping\n");
				exit(EXIT_FAILURE);
			}
		} else {
			out.push_back(geom[i]);
		}
	}

	return out;
}

static void dump(std::vector<mvt_geometry> geom) {
	for (size_t i = 0; i < geom.size(); i++) {
		printf("%d %lld,%lld %ld %s\n", geom[i].op, geom[i].x, geom[i].y, geom[i].id, geom[i].phantom ? "true" : "false");
	}
}

static std::vector<mvt_geometry> remove_noop(std::vector<mvt_geometry> geom, std::vector<mvt_geometry> orig) {
	std::vector<mvt_geometry> out;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == mvt_moveto && i + 1 < geom.size() && geom[i + 1].op == mvt_moveto) {
			if (geom[i].id != 0) {
#if 0
				fprintf(stderr, "Removing a moveto with an id %ld\n", geom[i].id);
				dump(geom);
				fprintf(stderr, "Orig was\n");
				dump(orig);
				exit(EXIT_FAILURE);
#endif
			}
		} else {
			out.push_back(geom[i]);
		}
	}

	return out;
}

void clip_lines_to_tile(std::vector<mvt_geometry> geom, mvt_tile &tile, long x, long y, long extent, long buffer, std::vector<mvt_geometry> orig) {
	geom = clip_lines(geom, x * extent - buffer, y * extent - buffer, (x + 1) * extent + buffer, (y + 1) * extent + buffer);
	geom = remove_noop(geom, orig);

	mvt_layer &nl = tile.layers[tile.layers.size() - 1];
	mvt_feature &nf = nl.features[nl.features.size() - 1];
	nf.geometry = geom;
}

void split_feature(mvt_layer const &layer, mvt_feature const &feature, std::vector<std::vector<mvt_tile>> &subtiles, size_t n) {
	static long clipid_pool = 0;
	const std::vector<mvt_geometry> &geom = feature.geometry;
	long extent = layer.extent;
	long nextent = extent / n;

	if (nextent * (long) n != extent) {
		fprintf(stderr, "Extent %ld doesn't subdivide evenly by %zud\n", extent, n);
		exit(EXIT_FAILURE);
	}

	// Calculate bounding box of feature

	long minx = LONG_MAX;
	long miny = LONG_MAX;
	long maxx = LONG_MIN;
	long maxy = LONG_MIN;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == mvt_moveto || geom[i].op == mvt_lineto) {
			if (geom[i].x < minx) {
				minx = geom[i].x;
			}
			if (geom[i].y < miny) {
				miny = geom[i].y;
			}
			if (geom[i].x > maxx) {
				maxx = geom[i].x;
			}
			if (geom[i].y > maxy) {
				maxy = geom[i].y;
			}
		}
	}

	// Extend bounding box by buffer

	long buffer = nextent * 5 / 256;  // XXX make configurable
	if (buffer <= 0) {
		fprintf(stderr, "buffer was eliminated\n");
		buffer = 1;
		exit(EXIT_FAILURE);
	}
	minx -= buffer;
	miny -= buffer;
	maxx += buffer;
	maxy += buffer;

	long left = floor((double) minx / nextent);
	long top = floor((double) miny / nextent);
	long right = floor((double) maxx / nextent);
	long bottom = floor((double) maxy / nextent);

	left = std::min(left, 0L);
	top = std::min(top, 0L);
	right = std::max(right, (long) (n - 1));
	bottom = std::max(right, (long) (n - 1));

	// Is it bigger than one sub-tile?
	// If so, generate an ID for matching
	// XXX Is this right for edges on the border?

	long nclipid = 0;
	if (left != right || top != bottom) {
		nclipid = ++clipid_pool;
	}

	// Set up a corresponding feature within each sub-tile
	// (Empty features will have to get torn down later)

	for (size_t x = 0; x < n; x++) {
		for (size_t y = 0; y < n; y++) {
			mvt_feature nf;

			nf.tags = feature.tags;
			nf.type = feature.type;
			nf.id = feature.id;
			nf.has_id = feature.has_id;
			nf.clipid = nclipid;

			mvt_tile &nt = subtiles[x][y];
			mvt_layer &nl = nt.layers[nt.layers.size() - 1];
			nl.features.push_back(nf);
		}
	}

	if (feature.type == mvt_linestring) {
		std::vector<mvt_geometry> ogeom = geom;
		std::vector<mvt_geometry> ngeom;
		long pointid = 0;

#if 0
		// Part 1: Assign (phantom) point IDs in the middle of any
		// segments that cross from one sub-tile to another

		for (size_t i = 0; i < ogeom.size(); i++) {
			if (i > 0 && ogeom[i].op == mvt_lineto && (floor((double) ogeom[i].x / nextent) != floor((double) ogeom[i - 1].x / nextent))) {
				long first = floor((double) ogeom[i - 1].x / nextent) * nextent;
				long second = floor((double) ogeom[i].x / nextent) * nextent;

				if (second > first) {
					for (long x = first + 1; x <= second; x += nextent) {
						long y = ogeom[i - 1].y + (double) (ogeom[i].y - ogeom[i - 1].y) * (x - first) / (ogeom[i].x - ogeom[i - 1].x);
						mvt_geometry p(ogeom[i].op, x, y);
						p.id = ++pointid;
						p.phantom = true;
						ngeom.push_back(p);
					}
				} else {
					for (long x = first; x >= second + 1; x -= nextent) {
						long y = ogeom[i - 1].y + (double) (ogeom[i].y - ogeom[i - 1].y) * (x - first) / (ogeom[i].x - ogeom[i - 1].x);
						mvt_geometry p(ogeom[i].op, x, y);
						p.id = ++pointid;
						p.phantom = true;
						ngeom.push_back(p);
					}
				}
			}

			ngeom.push_back(ogeom[i]);
		}

		// Part 1a: same, but for Y axis crossings

		ogeom = ngeom;
		ngeom.clear();
		for (size_t i = 0; i < ogeom.size(); i++) {
			if (i > 0 && ogeom[i].op == mvt_lineto && (floor((double) ogeom[i].y / nextent) != floor((double) ogeom[i - 1].y / nextent))) {
				long first = (ogeom[i - 1].y / nextent) * nextent;
				long second = (ogeom[i].y / nextent) * nextent;

				if (second > first) {
					for (long y = first + 1; y <= second; y += nextent) {
						long x = ogeom[i - 1].x + (double) (ogeom[i].x - ogeom[i - 1].x) * (y - first) / (ogeom[i].y - ogeom[i - 1].y);
						mvt_geometry p(ogeom[i].op, x, y);
						p.id = ++pointid;
						p.phantom = true;
						ngeom.push_back(p);
					}
				} else {
					for (long y = first; y >= second + 1; y -= nextent) {
						long x = ogeom[i - 1].x + (double) (ogeom[i].x - ogeom[i - 1].x) * (y - first) / (ogeom[i].y - ogeom[i - 1].y);
						mvt_geometry p(ogeom[i].op, x, y);
						p.id = ++pointid;
						p.phantom = true;
						ngeom.push_back(p);
					}
				}
			}

			ngeom.push_back(ogeom[i]);
		}
#endif
		ngeom = ogeom;

		// Part 2: Assign (real) point IDs for any points that are on a sub-tile edge
		for (size_t i = 0; i < ngeom.size(); i++) {
			if (ngeom[i].x % nextent == 0 || ngeom[i].y % nextent == 0) {
				if (ngeom[i].id == 0) {
					ngeom[i].id = ++pointid;
					ngeom[i].phantom = false;
				}
			}
		}

		// Part 3: Clip the geometry to each of the sub-tiles

		for (size_t x = 0; x < n; x++) {
			for (size_t y = 0; y < n; y++) {
				clip_lines_to_tile(ngeom, subtiles[x][y], x, y, nextent, buffer, geom);
			}
		}
	} else {
		// deal with other feature types later

		mvt_tile &nt = subtiles[0][0];
		mvt_layer &nl = nt.layers[nt.layers.size() - 1];
		mvt_feature &nf = nl.features[nl.features.size() - 1];
		nf.geometry = geom;
	}
}

mvt_tile split_and_merge(mvt_tile tile, int tile_zoom) {
	// Features will be split into an NxN grid of sub-tiles,
	// to be merged back together at the end,
	// which should result in the original set of features
	// except (perhaps) for their sequence.

	size_t n = 1 << tile_zoom;
	std::vector<std::vector<mvt_tile>> subtiles;
	subtiles.resize(n);
	for (size_t i = 0; i < n; i++) {
		subtiles[i].resize(n);
	}

	for (size_t i = 0; i < tile.layers.size(); i++) {
		mvt_layer &layer = tile.layers[i];

		// Set up a corresponding layer within each sub-tile
		// (Empty layers will have to get torn down later)

		for (size_t x = 0; x < n; x++) {
			for (size_t y = 0; y < n; y++) {
				mvt_layer nl;

				// Note that for simplicity this is copying *all* the
				// keys and values to the sub-layers, not only the ones
				// actually needed for the sub-features.

				nl.version = layer.version;
				nl.extent = layer.extent; // >> tile_zoom;
				nl.name = layer.name;
				nl.keys = layer.keys;
				nl.values = layer.values;

				subtiles[x][y].layers.push_back(nl);
			}
		}

		// Iterate through features, copying them into sub-tiles

		for (size_t j = 0; j < layer.features.size(); j++) {
			mvt_feature &feature = layer.features[j];

			split_feature(layer, feature, subtiles, n);
		}
	}

	// Trim unused features from layers, layers from tiles

	// Write each tile to PBF
	// Decode each tile back from PBF

	// Recreate original tile from decoded sub-tiles

	tile.layers.clear();
	for (size_t x = 0; x < n; x++) {
		for (size_t y = 0; y < n; y++) {
			for (size_t k = 0; k < subtiles[x][y].layers.size(); k++) {
				tile.layers.push_back(subtiles[x][y].layers[k]);
			}
		}
	}

	// Verify that the original data has been recreated

	return tile;
}
