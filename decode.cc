#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string>
#include <zlib.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include "vector_tile.pb.h"
#include "tile.h"

extern "C" {
#include "projection.h"
}

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
inline bool is_compressed(std::string const &data) {
	return data.size() > 2 && (((uint8_t) data[0] == 0x78 && (uint8_t) data[1] == 0x9C) || ((uint8_t) data[0] == 0x1F && (uint8_t) data[1] == 0x8B));
}

// https://github.com/mapbox/mapnik-vector-tile/blob/master/src/vector_tile_compression.hpp
inline int decompress(std::string const &input, std::string &output) {
	z_stream inflate_s;
	inflate_s.zalloc = Z_NULL;
	inflate_s.zfree = Z_NULL;
	inflate_s.opaque = Z_NULL;
	inflate_s.avail_in = 0;
	inflate_s.next_in = Z_NULL;
	if (inflateInit2(&inflate_s, 32 + 15) != Z_OK) {
		fprintf(stderr, "error: %s\n", inflate_s.msg);
	}
	inflate_s.next_in = (Bytef *) input.data();
	inflate_s.avail_in = input.size();
	size_t length = 0;
	do {
		output.resize(length + 2 * input.size());
		inflate_s.avail_out = 2 * input.size();
		inflate_s.next_out = (Bytef *) (output.data() + length);
		int ret = inflate(&inflate_s, Z_FINISH);
		if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR) {
			fprintf(stderr, "error: %s\n", inflate_s.msg);
			return 0;
		}

		length += (2 * input.size() - inflate_s.avail_out);
	} while (inflate_s.avail_out == 0);
	inflateEnd(&inflate_s);
	output.resize(length);
	return 1;
}

int dezig(unsigned n) {
	return (n >> 1) ^ (-(n & 1));
}

void printq(const char *s) {
	putchar('"');
	for (; *s; s++) {
		if (*s == '\\' || *s == '"') {
			printf("\\%c", *s);
		} else if (*s >= 0 && *s < ' ') {
			printf("\\u%04x", *s);
		} else {
			putchar(*s);
		}
	}
	putchar('"');
}

struct draw {
	int op;
	double lon;
	double lat;

	draw(int op, double lon, double lat) {
		this->op = op;
		this->lon = lon;
		this->lat = lat;
	}
};

void handle(std::string message, int z, unsigned x, unsigned y, int describe) {
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	int within = 0;

	// https://github.com/mapbox/mapnik-vector-tile/blob/master/examples/c%2B%2B/tileinfo.cpp
	mapnik::vector::tile tile;

	if (is_compressed(message)) {
		std::string uncompressed;
		decompress(message, uncompressed);
		google::protobuf::io::ArrayInputStream stream(uncompressed.c_str(), uncompressed.length());
		google::protobuf::io::CodedInputStream codedstream(&stream);
		codedstream.SetTotalBytesLimit(10 * 67108864, 5 * 67108864);
		if (!tile.ParseFromCodedStream(&codedstream)) {
			fprintf(stderr, "Couldn't decompress tile %d/%u/%u\n", z, x, y);
			exit(EXIT_FAILURE);
		}
	} else if (!tile.ParseFromString(message)) {
		fprintf(stderr, "Couldn't parse tile %d/%u/%u\n", z, x, y);
		exit(EXIT_FAILURE);
	}

	printf("{ \"type\": \"FeatureCollection\"");

	if (describe) {
		printf(", \"properties\": { \"zoom\": %d, \"x\": %d, \"y\": %d }", z, x, y);
	}

	printf(", \"features\": [\n");

	for (int l = 0; l < tile.layers_size(); l++) {
		mapnik::vector::tile_layer layer = tile.layers(l);
		int extent = layer.extent();

		if (describe) {
			if (l != 0) {
				printf(",\n");
			}

			printf("{ \"type\": \"FeatureCollection\"");
			printf(", \"properties\": { \"layer\": ");
			printq(layer.name().c_str());
			printf(" }");
			printf(", \"features\": [\n");

			within = 0;
		}

		for (int f = 0; f < layer.features_size(); f++) {
			mapnik::vector::tile_feature feat = layer.features(f);
			int px = 0, py = 0;

			if (within) {
				printf(",\n");
			}
			within = 1;

			printf("{ \"type\": \"Feature\"");
			printf(", \"properties\": { ");

			for (int t = 0; t + 1 < feat.tags_size(); t += 2) {
				if (t != 0) {
					printf(", ");
				}

				const char *key = layer.keys(feat.tags(t)).c_str();
				mapnik::vector::tile_value const &val = layer.values(feat.tags(t + 1));

				if (val.has_string_value()) {
					printq(key);
					printf(": ");
					printq(val.string_value().c_str());
				} else if (val.has_int_value()) {
					printq(key);
					printf(": %lld", (long long) val.int_value());
				} else if (val.has_double_value()) {
					printq(key);
					double v = val.double_value();
					if (v == (long long) v) {
						printf(": %lld", (long long) v);
					} else {
						printf(": %g", v);
					}
				} else if (val.has_float_value()) {
					printq(key);
					double v = val.float_value();
					if (v == (long long) v) {
						printf(": %lld", (long long) v);
					} else {
						printf(": %g", v);
					}
				} else if (val.has_sint_value()) {
					printq(key);
					printf(": %lld", (long long) val.sint_value());
				} else if (val.has_uint_value()) {
					printq(key);
					printf(": %lld", (long long) val.uint_value());
				} else if (val.has_bool_value()) {
					printq(key);
					printf(": %s", val.bool_value() ? "true" : "false");
				}
			}

			printf(" }, \"geometry\": { ");

			std::vector<draw> ops;

			for (int g = 0; g < feat.geometry_size(); g++) {
				uint32_t geom = feat.geometry(g);
				uint32_t op = geom & 7;
				uint32_t count = geom >> 3;

				if (op == VT_MOVETO || op == VT_LINETO) {
					for (unsigned k = 0; k < count; k++) {
						px += dezig(feat.geometry(g + 1));
						py += dezig(feat.geometry(g + 2));
						g += 2;

						long long scale = 1LL << (32 - z);
						long long wx = scale * x + (scale / extent) * px;
						long long wy = scale * y + (scale / extent) * py;

						double lat, lon;
						tile2latlon(wx, wy, 32, &lat, &lon);

						ops.push_back(draw(op, lon, lat));
					}
				} else {
					ops.push_back(draw(op, 0, 0));
				}
			}

			if (feat.type() == VT_POINT) {
				if (ops.size() == 1) {
					printf("\"type\": \"Point\", \"coordinates\": [ %f, %f ]", ops[0].lon, ops[0].lat);
				} else {
					printf("\"type\": \"MultiPoint\", \"coordinates\": [ ");
					for (unsigned i = 0; i < ops.size(); i++) {
						if (i != 0) {
							printf(", ");
						}
						printf("[ %f, %f ]", ops[i].lon, ops[i].lat);
					}
					printf(" ]");
				}
			} else if (feat.type() == VT_LINE) {
				int movetos = 0;
				for (unsigned i = 0; i < ops.size(); i++) {
					if (ops[i].op == VT_MOVETO) {
						movetos++;
					}
				}

				if (movetos < 2) {
					printf("\"type\": \"LineString\", \"coordinates\": [ ");
					for (unsigned i = 0; i < ops.size(); i++) {
						if (i != 0) {
							printf(", ");
						}
						printf("[ %f, %f ]", ops[i].lon, ops[i].lat);
					}
					printf(" ]");
				} else {
					printf("\"type\": \"MultiLineString\", \"coordinates\": [ [ ");
					int state = 0;
					for (unsigned i = 0; i < ops.size(); i++) {
						if (ops[i].op == VT_MOVETO) {
							if (state == 0) {
								printf("[ %f, %f ]", ops[i].lon, ops[i].lat);
								state = 1;
							} else {
								printf(" ], [ ");
								printf("[ %f, %f ]", ops[i].lon, ops[i].lat);
								state = 1;
							}
						} else {
							printf(", [ %f, %f ]", ops[i].lon, ops[i].lat);
						}
					}
					printf(" ] ]");
				}
			} else if (feat.type() == VT_POLYGON) {
				std::vector<std::vector<draw> > rings;
				std::vector<double> areas;

				for (unsigned i = 0; i < ops.size(); i++) {
					if (ops[i].op == VT_MOVETO) {
						rings.push_back(std::vector<draw>());
						areas.push_back(0);
					}

					int n = rings.size() - 1;
					if (n >= 0) {
						rings[n].push_back(ops[i]);
					}
				}

				int outer = 0;

				for (unsigned i = 0; i < rings.size(); i++) {
					double area = 0;
					for (unsigned k = 0; k < rings[i].size(); k++) {
						if (rings[i][k].op != VT_CLOSEPATH) {
							area += rings[i][k].lon * rings[i][(k + 1) % rings[i].size()].lat;
							area -= rings[i][k].lat * rings[i][(k + 1) % rings[i].size()].lon;
						}
					}

					areas[i] = area;
					if (areas[i] <= 0 || i == 0) {
						outer++;
					}

					// printf("area %f\n", area / .00000274 / .00000274);
				}

				if (outer > 1) {
					printf("\"type\": \"MultiPolygon\", \"coordinates\": [ [ [ ");
				} else {
					printf("\"type\": \"Polygon\", \"coordinates\": [ [ ");
				}

				int state = 0;
				for (unsigned i = 0; i < rings.size(); i++) {
					if (areas[i] <= 0) {
						if (state != 0) {
							// new multipolygon
							printf(" ] ], [ [ ");
						}
						state = 1;
					}

					if (state == 2) {
						// new ring in the same polygon
						printf(" ], [ ");
					}

					for (unsigned j = 0; j < rings[i].size(); j++) {
						if (rings[i][j].op != VT_CLOSEPATH) {
							if (j != 0) {
								printf(", ");
							}

							printf("[ %f, %f ]", rings[i][j].lon, rings[i][j].lat);
						} else {
							if (j != 0) {
								printf(", ");
							}

							printf("[ %f, %f ]", rings[i][0].lon, rings[i][0].lat);
						}
					}

					state = 2;
				}

				if (outer > 1) {
					printf(" ] ] ]");
				} else {
					printf(" ] ]");
				}
			}

			printf(" } }\n");
		}

		if (describe) {
			printf("] }\n");
		}
	}

	printf("] }\n");
}

void decode(char *fname, int z, unsigned x, unsigned y) {
	sqlite3 *db;
	int oz = z;
	unsigned ox = x, oy = y;

	int fd = open(fname, O_RDONLY);
	if (fd >= 0) {
		struct stat st;
		if (fstat(fd, &st) == 0) {
			if (st.st_size < 50 * 1024 * 1024) {
				char *map = (char *) mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
				if (map != NULL && map != MAP_FAILED) {
					if (strcmp(map, "SQLite format 3") != 0) {
						std::string s = std::string(map, st.st_size);
						handle(s, z, x, y, 1);
						munmap(map, st.st_size);
						return;
					}
				}
				munmap(map, st.st_size);
			}
		} else {
			perror("fstat");
		}
		close(fd);
	} else {
		perror(fname);
	}

	if (sqlite3_open(fname, &db) != SQLITE_OK) {
		fprintf(stderr, "%s: %s\n", fname, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	if (z < 0) {
		printf("{ \"type\": \"FeatureCollection\", \"properties\": {\n");

		const char *sql2 = "SELECT name, value from metadata order by name;";
		sqlite3_stmt *stmt2;
		if (sqlite3_prepare_v2(db, sql2, -1, &stmt2, NULL) != SQLITE_OK) {
			fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}

		int within = 0;
		while (sqlite3_step(stmt2) == SQLITE_ROW) {
			if (within) {
				printf(",\n");
			}
			within = 1;

			const unsigned char *name = sqlite3_column_text(stmt2, 0);
			const unsigned char *value = sqlite3_column_text(stmt2, 1);

			printq((char *) name);
			printf(": ");
			printq((char *) value);
		}

		sqlite3_finalize(stmt2);

		const char *sql = "SELECT tile_data, zoom_level, tile_column, tile_row from tiles order by zoom_level, tile_column, tile_row;";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
			fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}

		printf("\n}, \"features\": [\n");

		within = 0;
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			if (within) {
				printf(",\n");
			}
			within = 1;

			int len = sqlite3_column_bytes(stmt, 0);
			int z = sqlite3_column_int(stmt, 1);
			int x = sqlite3_column_int(stmt, 2);
			int y = sqlite3_column_int(stmt, 3);
			y = (1LL << z) - 1 - y;
			const char *s = (const char *) sqlite3_column_blob(stmt, 0);

			handle(std::string(s, len), z, x, y, 1);
		}

		printf("] }\n");

		sqlite3_finalize(stmt);
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

				handle(std::string(s, len), z, x, y, 0);
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
	fprintf(stderr, "Usage: %s file.mbtiles zoom x y\n", argv[0]);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	extern int optind;
	// extern char *optarg;
	int i;

	while ((i = getopt(argc, argv, "")) != -1) {
		usage(argv);
	}

	if (argc == optind + 4) {
		decode(argv[optind], atoi(argv[optind + 1]), atoi(argv[optind + 2]), atoi(argv[optind + 3]));
	} else if (argc == optind + 1) {
		decode(argv[optind], -1, -1, -1);
	} else {
		usage(argv);
	}

	return 0;
}
