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
#include "mvt.hpp"
#include "projection.hpp"
#include "geometry.hpp"
#include "write_json.hpp"
#include "jsonpull/jsonpull.h"
#include "dirtiles.hpp"

int minzoom = 0;
int maxzoom = 32;
bool force = false;

void do_stats(mvt_tile &tile, size_t size, bool compressed, int z, unsigned x, unsigned y, json_writer &jw) {
	jw.begin_hash();

	jw.write_string("zoom");
	jw.write_signed(z);

	jw.write_string("x");
	jw.write_unsigned(x);

	jw.write_string("y");
	jw.write_unsigned(y);

	jw.write_string("bytes");
	jw.write_unsigned(size);

	jw.write_string("compressed");
	jw.write_bool(compressed);

	jw.write_string("layers");
	jw.begin_hash();

	for (size_t i = 0; i < tile.layers.size(); i++) {
		jw.write_string(tile.layers[i].name);

		size_t points = 0, lines = 0, polygons = 0;
		for (size_t j = 0; j < tile.layers[i].features.size(); j++) {
			if (tile.layers[i].features[j].type == mvt_point) {
				points++;
			} else if (tile.layers[i].features[j].type == mvt_linestring) {
				lines++;
			} else if (tile.layers[i].features[j].type == mvt_polygon) {
				polygons++;
			}
		}

		jw.begin_hash();

		jw.write_string("points");
		jw.write_unsigned(points);

		jw.write_string("lines");
		jw.write_unsigned(lines);

		jw.write_string("polygons");
		jw.write_unsigned(polygons);

		jw.write_string("extent");
		jw.write_signed(tile.layers[i].extent);

		jw.end_hash();
	}

	jw.end_hash();
	jw.end_hash();

	jw.write_newline();
}

void handle(std::string message, int z, unsigned x, unsigned y, std::set<std::string> const &to_decode, bool pipeline, bool stats, json_writer &jw) {
	mvt_tile tile;
	bool was_compressed;

	try {
		if (!tile.decode(message, was_compressed)) {
			fprintf(stderr, "Couldn't parse tile %d/%u/%u\n", z, x, y);
			exit(EXIT_FAILURE);
		}
	} catch (std::exception const &e) {
		fprintf(stderr, "PBF decoding error in tile %d/%u/%u\n", z, x, y);
		exit(EXIT_FAILURE);
	}

	if (stats) {
		do_stats(tile, message.size(), was_compressed, z, x, y, jw);
		return;
	}

	if (!pipeline) {
		jw.begin_hash();

		jw.write_string("type");
		jw.write_string("FeatureCollection");

		if (true) {
			jw.write_string("properties");
			jw.begin_hash();

			jw.write_string("zoom");
			jw.write_signed(z);

			jw.write_string("x");
			jw.write_signed(x);

			jw.write_string("y");
			jw.write_signed(y);

			if (!was_compressed) {
				jw.write_string("compressed");
				jw.write_bool(false);
			}

			jw.end_hash();

			if (projection != projections) {
				jw.write_string("crs");
				jw.begin_hash();

				jw.write_string("type");
				jw.write_string("name");

				jw.write_string("properties");
				jw.begin_hash();

				jw.write_string("name");
				jw.write_string(projection->alias);

				jw.end_hash();
				jw.end_hash();
			}
		}

		jw.write_string("features");
		jw.begin_array();
		jw.write_newline();
	}

	bool first_layer = true;
	for (size_t l = 0; l < tile.layers.size(); l++) {
		mvt_layer &layer = tile.layers[l];

		if (layer.extent <= 0) {
			fprintf(stderr, "Impossible layer extent %lld in mbtiles\n", layer.extent);
			exit(EXIT_FAILURE);
		}

		if (to_decode.size() != 0 && !to_decode.count(layer.name)) {
			continue;
		}

		if (!pipeline) {
			if (true) {
				if (!first_layer) {
					jw.json_comma_newline();
				}

				jw.begin_hash();

				jw.write_string("type");
				jw.write_string("FeatureCollection");

				jw.write_string("properties");
				jw.begin_hash();

				jw.write_string("layer");
				jw.write_string(layer.name);

				jw.write_string("version");
				jw.write_signed(layer.version);

				jw.write_string("extent");
				jw.write_signed(layer.extent);

				jw.end_hash();

				jw.write_string("features");
				jw.begin_array();

				jw.write_newline();
				first_layer = false;
			}
		}

		// X and Y are unsigned, so no need to check <0
		if (x > (1ULL << z) || y > (1ULL << z)) {
			fprintf(stderr, "Impossible tile %d/%u/%u\n", z, x, y);
			exit(EXIT_FAILURE);
		}

		layer_to_geojson(layer, z, x, y, !pipeline, pipeline, pipeline, false, 0, 0, 0, !force, jw);

		if (!pipeline) {
			if (true) {
				jw.end_array();
				jw.end_hash();
				jw.write_newline();
			}
		}
	}

	if (!pipeline) {
		jw.end_array();
		jw.end_hash();
		jw.write_newline();
	}
}

void decode(char *fname, int z, unsigned x, unsigned y, std::set<std::string> const &to_decode, bool pipeline, bool stats, std::set<std::string> const &exclude_meta) {
	sqlite3 *db = NULL;
	bool isdir = false;
	int oz = z;
	unsigned ox = x, oy = y;
	json_writer jw(stdout);

	int fd = open(fname, O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		struct stat st;
		if (fstat(fd, &st) == 0) {
			if (st.st_size < 50 * 1024 * 1024) {
				char *map = (char *) mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
				if (map != NULL && map != MAP_FAILED) {
					if (strcmp(map, "SQLite format 3") != 0) {
						if (z >= 0) {
							std::string s = std::string(map, st.st_size);
							handle(s, z, x, y, to_decode, pipeline, stats, jw);
							munmap(map, st.st_size);
							return;
						} else {
							fprintf(stderr, "Must specify zoom/x/y to decode a single pbf file\n");
							exit(EXIT_FAILURE);
						}
					}
				}
				munmap(map, st.st_size);
			}
		} else {
			perror("fstat");
		}
		if (close(fd) != 0) {
			perror("close");
			exit(EXIT_FAILURE);
		}
	} else {
		perror(fname);
	}

	struct stat st;
	std::vector<zxy> tiles;
	if (stat(fname, &st) == 0 && (st.st_mode & S_IFDIR) != 0) {
		isdir = true;

		db = dirmeta2tmp(fname);
		tiles = enumerate_dirtiles(fname, minzoom, maxzoom);
	} else {
		if (sqlite3_open(fname, &db) != SQLITE_OK) {
			fprintf(stderr, "%s: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}

		char *err = NULL;
		if (sqlite3_exec(db, "PRAGMA integrity_check;", NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "%s: integrity_check: %s\n", fname, err);
			exit(EXIT_FAILURE);
		}
	}

	if (z < 0) {
		int within = 0;

		if (!pipeline && !stats) {
			jw.begin_hash();

			jw.write_string("type");
			jw.write_string("FeatureCollection");

			jw.write_string("properties");
			jw.begin_hash();
			jw.write_newline();

			const char *sql2 = "SELECT name, value from metadata order by name;";
			sqlite3_stmt *stmt2;
			if (sqlite3_prepare_v2(db, sql2, -1, &stmt2, NULL) != SQLITE_OK) {
				fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
				exit(EXIT_FAILURE);
			}

			while (sqlite3_step(stmt2) == SQLITE_ROW) {
				const unsigned char *name = sqlite3_column_text(stmt2, 0);
				const unsigned char *value = sqlite3_column_text(stmt2, 1);

				if (exclude_meta.count((char *) name) == 0) {
					if (within) {
						jw.json_comma_newline();
					}
					within = 1;

					if (name == NULL || value == NULL) {
						fprintf(stderr, "Corrupt mbtiles file: null metadata\n");
						exit(EXIT_FAILURE);
					}

					jw.write_string((char *) name);
					jw.write_string((char *) value);
				}
			}

			jw.write_newline();
			jw.wantnl = false;  // XXX

			sqlite3_finalize(stmt2);
		}

		if (stats) {
			jw.begin_array();
			jw.write_newline();
		}

		if (!pipeline && !stats) {
			jw.end_hash();

			jw.write_string("features");
			jw.begin_array();
			jw.write_newline();
		}

		if (isdir) {
			within = 0;
			for (size_t i = 0; i < tiles.size(); i++) {
				if (!pipeline && !stats) {
					if (within) {
						jw.json_comma_newline();
					}
					within = 1;
				}
				if (stats) {
					if (within) {
						jw.json_comma_newline();
					}
					within = 1;
				}

				std::string fn = std::string(fname) + "/" + tiles[i].path();
				FILE *f = fopen(fn.c_str(), "rb");
				if (f == NULL) {
					perror(fn.c_str());
					exit(EXIT_FAILURE);
				}

				std::string s;
				char buf[2000];
				ssize_t n;
				while ((n = fread(buf, 1, 2000, f)) > 0) {
					s.append(std::string(buf, n));
				}
				fclose(f);

				handle(s, tiles[i].z, tiles[i].x, tiles[i].y, to_decode, pipeline, stats, jw);
			}
		} else {
			const char *sql = "SELECT tile_data, zoom_level, tile_column, tile_row from tiles where zoom_level between ? and ? order by zoom_level, tile_column, tile_row;";
			sqlite3_stmt *stmt;
			if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
				fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
				exit(EXIT_FAILURE);
			}

			sqlite3_bind_int(stmt, 1, minzoom);
			sqlite3_bind_int(stmt, 2, maxzoom);

			within = 0;
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				if (!pipeline && !stats) {
					if (within) {
						jw.json_comma_newline();
					}
					within = 1;
				}
				if (stats) {
					if (within) {
						jw.json_comma_newline();
					}
					within = 1;
				}

				int len = sqlite3_column_bytes(stmt, 0);
				int tz = sqlite3_column_int(stmt, 1);
				int tx = sqlite3_column_int(stmt, 2);
				int ty = sqlite3_column_int(stmt, 3);

				if (tz < 0 || tz >= 32) {
					fprintf(stderr, "Impossible zoom level %d in mbtiles\n", tz);
					exit(EXIT_FAILURE);
				}

				ty = (1LL << tz) - 1 - ty;
				const char *s = (const char *) sqlite3_column_blob(stmt, 0);

				handle(std::string(s, len), tz, tx, ty, to_decode, pipeline, stats, jw);
			}

			sqlite3_finalize(stmt);
		}

		if (!pipeline && !stats) {
			jw.end_array();
			jw.end_hash();
			jw.write_newline();
		}
		if (stats) {
			jw.end_array();
			jw.write_newline();
		}
		if (pipeline) {
			jw.write_newline();
		}
	} else {
		int handled = 0;
		while (z >= 0 && !handled) {
			const char *sql = "SELECT tile_data from tiles where zoom_level = ? and tile_column = ? and tile_row = ?;";
			sqlite3_stmt *stmt;
			if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
				fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
				exit(EXIT_FAILURE);
			}

			sqlite3_bind_int(stmt, 1, z);
			sqlite3_bind_int(stmt, 2, x);
			sqlite3_bind_int(stmt, 3, (1LL << z) - 1 - y);

			while (sqlite3_step(stmt) == SQLITE_ROW) {
				int len = sqlite3_column_bytes(stmt, 0);
				const char *s = (const char *) sqlite3_column_blob(stmt, 0);

				if (z != oz) {
					fprintf(stderr, "%s: Warning: using tile %d/%u/%u instead of %d/%u/%u\n", fname, z, x, y, oz, ox, oy);
				}

				handle(std::string(s, len), z, x, y, to_decode, pipeline, stats, jw);
				handled = 1;
			}

			sqlite3_finalize(stmt);

			z--;
			x /= 2;
			y /= 2;
		}
	}

	if (sqlite3_close(db) != SQLITE_OK) {
		fprintf(stderr, "%s: could not close database: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s [-s projection] [-Z minzoom] [-z maxzoom] [-l layer ...] file.mbtiles [zoom x y]\n", argv[0]);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	extern int optind;
	extern char *optarg;
	int i;
	std::set<std::string> to_decode;
	bool pipeline = false;
	bool stats = false;
	std::set<std::string> exclude_meta;

	struct option long_options[] = {
		{"projection", required_argument, 0, 's'},
		{"maximum-zoom", required_argument, 0, 'z'},
		{"minimum-zoom", required_argument, 0, 'Z'},
		{"layer", required_argument, 0, 'l'},
		{"tag-layer-and-zoom", no_argument, 0, 'c'},
		{"stats", no_argument, 0, 'S'},
		{"force", no_argument, 0, 'f'},
		{"exclude-metadata-row", required_argument, 0, 'x'},
		{0, 0, 0, 0},
	};

	std::string getopt_str;
	for (size_t lo = 0; long_options[lo].name != NULL; lo++) {
		if (long_options[lo].val > ' ') {
			getopt_str.push_back(long_options[lo].val);

			if (long_options[lo].has_arg == required_argument) {
				getopt_str.push_back(':');
			}
		}
	}

	while ((i = getopt_long(argc, argv, getopt_str.c_str(), long_options, NULL)) != -1) {
		switch (i) {
		case 0:
			break;

		case 's':
			set_projection_or_exit(optarg);
			break;

		case 'z':
			maxzoom = atoi(optarg);
			break;

		case 'Z':
			minzoom = atoi(optarg);
			break;

		case 'l':
			to_decode.insert(optarg);
			break;

		case 'c':
			pipeline = true;
			break;

		case 'S':
			stats = true;
			break;

		case 'f':
			force = true;
			break;

		case 'x':
			exclude_meta.insert(optarg);
			break;

		default:
			usage(argv);
		}
	}

	if (argc == optind + 4) {
		decode(argv[optind], atoi(argv[optind + 1]), atoi(argv[optind + 2]), atoi(argv[optind + 3]), to_decode, pipeline, stats, exclude_meta);
	} else if (argc == optind + 1) {
		decode(argv[optind], -1, -1, -1, to_decode, pipeline, stats, exclude_meta);
	} else {
		usage(argv);
	}

	return 0;
}
