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

	bool inside = false;

	for (size_t i = 0; i < geom.size(); i++) {
		if (geom[i].op == mvt_moveto) {
			if (geom[i].x >= left && geom[i].x <= right && geom[i].y >= top && geom[i].y <= bottom) {
				// First point is inside.

				out.push_back(geom[i]);
				inside = true;
			} else {
				inside = false;
			}
		} else {
			if (geom[i].x >= left && geom[i].x <= right && geom[i].y >= top && geom[i].y <= bottom) {
				// Next point is inside.

				if (inside) {
					// Moving from inside to another inside, so just include the new point
					out.push_back(geom[i]);
				} else {
					// Moving from outside to inside, so clip and include the inner portion
					double x1 = geom[i - 1].x;
					double y1 = geom[i - 1].y;

					double x2 = geom[i - 0].x;
					double y2 = geom[i - 0].y;

					int c = clip(&x1, &y1, &x2, &y2, left, top, right, bottom);
					if (c != CLIP_CHANGED_FIRST) {
						fprintf(stderr, "Expected first to change\n");
						exit(EXIT_FAILURE);
					}

					mvt_geometry phantom(mvt_moveto, round(x1), round(y1));
					phantom.phantom = true;
					phantom.id = 0;
					out.push_back(phantom);
					out.push_back(geom[i]);
				}

				inside = true;
			} else {
				// Next point is outside.

				if (inside) {
					// Moving from inside to outside, so clip and include the inner portion
					double x1 = geom[i - 1].x;
					double y1 = geom[i - 1].y;

					double x2 = geom[i - 0].x;
					double y2 = geom[i - 0].y;

					int c = clip(&x1, &y1, &x2, &y2, left, top, right, bottom);
					if (c != CLIP_CHANGED_SECOND) {
						fprintf(stderr, "Expected second to change\n");
						exit(EXIT_FAILURE);
					}

					// Inside already drawing, so don't need the start point
					// out.push_back(geom[i - 1]);

					mvt_geometry phantom(mvt_lineto, round(x2), round(y2));
					phantom.phantom = true;
					phantom.id = 0;
					out.push_back(phantom);
				} else {
					// Outside to outside, so discardable on both sides

					double x1 = geom[i - 1].x;
					double y1 = geom[i - 1].y;

					double x2 = geom[i - 0].x;
					double y2 = geom[i - 0].y;

					int c = clip(&x1, &y1, &x2, &y2, left, top, right, bottom);

					if (c != CLIP_ELIMINATED) {
						mvt_geometry phantom1(mvt_moveto, round(x1), round(y1));
						phantom1.phantom = true;
						phantom1.id = 0;
						out.push_back(phantom1);

						mvt_geometry phantom2(mvt_lineto, round(x2), round(y2));
						phantom2.phantom = true;
						phantom2.id = 0;
						out.push_back(phantom2);
					}
				}

				inside = false;
			}
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
		if (geom[i].op == mvt_moveto && (i + 1 >= geom.size() || geom[i + 1].op == mvt_moveto)) {
			if (geom[i].id != 0) {
				fprintf(stderr, "Removing a moveto with an id %ld in %zu\n", geom[i].id, geom.size());
				dump(geom);
				fprintf(stderr, "Orig was\n");
				dump(orig);
				exit(EXIT_FAILURE);
			}

			fprintf(stderr, "Discarded something\n");
			exit(EXIT_FAILURE);
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

	left = std::max(left, 0L);
	top = std::max(top, 0L);
	right = std::min(right, (long) (n - 1));
	bottom = std::min(bottom, (long) (n - 1));

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

		// Part 1: Assign (phantom) point IDs in the middle of any
		// segments that cross from one sub-tile to another

		for (size_t i = 0; i < ogeom.size(); i++) {
			if (i > 0 && ogeom[i].op == mvt_lineto && (floor((double) ogeom[i].x / nextent) != floor((double) ogeom[i - 1].x / nextent))) {
				long first = floor((double) ogeom[i - 1].x / nextent) * nextent;
				long second = floor((double) ogeom[i].x / nextent) * nextent;

				if (second >= first) {
					for (long x = first + nextent; x <= second; x += nextent) {
						long y = round(ogeom[i - 1].y + (ogeom[i].y - ogeom[i - 1].y) * (double) (x - ogeom[i - 1].x) / (ogeom[i].x - ogeom[i - 1].x));
						mvt_geometry p(ogeom[i].op, x, y);
						p.id = ++pointid;
						p.phantom = true;
						ngeom.push_back(p);
					}
				} else {
					for (long x = first; x >= second + nextent; x -= nextent) {
						long y = round(ogeom[i - 1].y + (ogeom[i].y - ogeom[i - 1].y) * (double) (x - ogeom[i - 1].x) / (ogeom[i].x - ogeom[i - 1].x));
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
				long first = floor((double) ogeom[i - 1].y / nextent) * nextent;
				long second = floor((double) ogeom[i].y / nextent) * nextent;

				if (second >= first) {
					for (long y = first + nextent; y <= second; y += nextent) {
						long x = round(ogeom[i - 1].x + (ogeom[i].x - ogeom[i - 1].x) * (double) (y - ogeom[i - 1].y) / (ogeom[i].y - ogeom[i - 1].y));
						mvt_geometry p(ogeom[i].op, x, y);
						p.id = ++pointid;
						p.phantom = true;
						ngeom.push_back(p);
					}
				} else {
					for (long y = first; y >= second + nextent; y -= nextent) {
						long x = round(ogeom[i - 1].x + (ogeom[i].x - ogeom[i - 1].x) * (double) (y - ogeom[i - 1].y) / (ogeom[i].y - ogeom[i - 1].y));
						mvt_geometry p(ogeom[i].op, x, y);
						p.id = ++pointid;
						p.phantom = true;
						ngeom.push_back(p);
					}
				}
			}

			ngeom.push_back(ogeom[i]);
		}

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

void trim_tile(mvt_tile &tile) {
	for (ssize_t i = tile.layers.size() - 1; i >= 0; i--) {
		mvt_layer &layer = tile.layers[i];

		for (ssize_t j = layer.features.size() - 1; j >= 0; j--) {
			mvt_feature &feature = layer.features[j];

			if (feature.geometry.size() == 0) {
				layer.features.erase(layer.features.begin() + j);
			}
		}

		if (layer.features.size() == 0) {
			tile.layers.erase(tile.layers.begin() + i);
		}
	}
}

mvt_feature *add_to_tile(mvt_feature const &f, mvt_layer const &l, mvt_tile &tile, size_t n) {
	size_t k;
	for (k = 0; k < tile.layers.size(); k++) {
		if (tile.layers[k].name == l.name) {
			break;
		}
	}

	if (k == tile.layers.size()) {
		mvt_layer nl = mvt_layer();
		nl.name = l.name;
		nl.extent = l.extent * n;
		tile.layers.push_back(nl);
	}

	mvt_feature nf;
	nf.type = f.type;
	nf.id = f.id;
	nf.has_id = f.has_id;
	nf.clipid = f.clipid;

	for (size_t kv = 0; kv + 1 < f.tags.size(); kv += 2) {
		tile.layers[k].tag(nf, l.keys[f.tags[kv]], l.values[f.tags[kv + 1]]);
	}

	tile.layers[k].features.push_back(nf);
	return &tile.layers[k].features[tile.layers[k].features.size() - 1];
}

struct partial {
	long clipid;
	const mvt_feature *f;
	const mvt_layer *l;
	long x;
	long y;

	bool operator<(const partial &p) const {
		return clipid < p.clipid;
	}
};

void merge_partials(std::vector<partial> &partials, size_t start, size_t end, mvt_tile &tile, size_t n) {
	// Pull out contiguous portions of original geometry

	std::vector<std::vector<mvt_geometry>> revised;

	for (size_t i = start; i < end; i++) {
		std::vector<mvt_geometry> const &geom = partials[i].f->geometry;

		for (size_t j = 0; j < geom.size(); j++) {
			if (geom[j].op == mvt_moveto) {
				size_t k;
				std::vector<mvt_geometry> out;
				out.push_back(geom[j]);

				for (k = j + 1; k < geom.size(); k++) {
					if (geom[k].op == mvt_moveto) {
						break;
					}
					out.push_back(geom[k]);
				}

				// Now need to discard anything that happens in the buffer,
				// because it should all be duplicated.

				// Do this by location instead of by ID, because geometry is
				// only tagged as phantom if it actually touches the tile
				// or buffer edge, not if it just happens to be in the buffer

				long minx = partials[i].l->extent * partials[i].x;
				long maxx = partials[i].l->extent * (partials[i].x + 1);
				long miny = partials[i].l->extent * partials[i].y;
				long maxy = partials[i].l->extent * (partials[i].y + 1);

				std::vector<mvt_geometry> out2;
				bool within = false;
				for (size_t l = 0; l < out.size(); l++) {
					if (out[l].x >= minx && out[l].x <= maxx && out[l].y >= miny && out[l].y <= maxy) {
						if (!within) {
							out[l].op = mvt_moveto;
						}
						out2.push_back(out[l]);
						within = true;
					} else {
						within = false;
					}
				}

				revised.push_back(out2);
				j = k - 1;
			}
		}
	}

	mvt_feature *nf = add_to_tile(*partials[start].f, *partials[start].l, tile, n);

#if 0
	for (size_t i = 0; i < revised.size(); i++) {
		for (size_t j = 0; j < revised[i].size(); j++) {
			nf->geometry.push_back(revised[i][j]);
		}
	}
#else

	// Pull out individual arcs from the various geometries

	std::multimap<long, std::vector<mvt_geometry>> arcs;

	for (size_t i = 0; i < revised.size(); i++) {
		for (size_t j = 0; j < revised[i].size(); j++) {
			std::vector<mvt_geometry> arc;
			arc.push_back(revised[i][j]);

			long id = revised[i][j].id;

			size_t k;
			for (k = j + 1; k < revised[i].size(); k++) {
				// Or would it be better to keep movetos together here,
				// as a way to chain to the next arc?
				if (revised[i][k].op == mvt_moveto) {
					break;
				}
				arc.push_back(revised[i][k]);
				if (revised[i][k].id != 0) {
					revised[i][k].op = mvt_moveto;
					break;
				}
			}

			arcs.insert(std::pair<long, std::vector<mvt_geometry>>(id, arc));
			j = k - 1;
		}
	}

	// Outer loop because removing an arc once it is used invalidates the iterator
	bool again = true;
	while (again) {
		again = false;

		for (auto a = arcs.begin(); a != arcs.end(); a++) {
			for (size_t j = 0; j < a->second.size(); j++) {
				nf->geometry.push_back(a->second[j]);
			}
		}
	}
#endif
}

mvt_tile reassemble(std::vector<std::vector<mvt_tile>> const &subtiles, size_t n) {
	mvt_tile tile;
	std::vector<partial> partials;

	for (size_t x = 0; x < n; x++) {
		for (size_t y = 0; y < n; y++) {
			mvt_tile const &t = subtiles[x][y];

			for (size_t i = 0; i < t.layers.size(); i++) {
				mvt_layer const &l = t.layers[i];

				for (size_t j = 0; j < l.features.size(); j++) {
					mvt_feature const &f = l.features[j];

					if (f.clipid == 0) {
						mvt_feature *nf = add_to_tile(f, l, tile, n);
						nf->geometry = f.geometry;
					} else {
						partial p;
						p.clipid = f.clipid;
						p.f = &f;
						p.l = &l;
						p.x = x;
						p.y = y;
						partials.push_back(p);
					}
				}
			}
		}
	}

	// XXX do partials

	std::sort(partials.begin(), partials.end());

	for (size_t i = 0; i < partials.size(); i++) {
		size_t j;
		for (j = i; j < partials.size(); j++) {
			if (partials[j].clipid != partials[i].clipid) {
				break;
			}
		}

		merge_partials(partials, i, j, tile, n);

		i = j - 1;
	}

	return tile;
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
				nl.extent = layer.extent >> tile_zoom;
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

	for (size_t x = 0; x < n; x++) {
		for (size_t y = 0; y < n; y++) {
			trim_tile(subtiles[x][y]);
		}
	}

	// Write each tile to PBF
	// Decode each tile back from PBF

	// Recreate original tile from decoded sub-tiles

	tile = reassemble(subtiles, n);

#if 0
	tile.layers.clear();
	for (size_t x = 0; x < n; x++) {
		for (size_t y = 0; y < n; y++) {
			for (size_t k = 0; k < subtiles[x][y].layers.size(); k++) {
				tile.layers.push_back(subtiles[x][y].layers[k]);
			}
		}
	}
#endif

	// Verify that the original data has been recreated

	return tile;
}
