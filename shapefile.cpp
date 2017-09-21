#include <stdlib.h>
#include "shapefile.hpp"
#include "mvt.hpp"
#include "serial.hpp"
#include "projection.hpp"
#include "main.hpp"
#include "milo/dtoa_milo.h"

static unsigned int read32le(unsigned char *ba) {
	return ((ba[0] & 0xFF)) |
	       ((ba[1] & 0xFF) << 8) |
	       ((ba[2] & 0xFF) << 16) |
	       ((ba[3] & 0xFF) << 24);
}

static unsigned int read16le(unsigned char *ba) {
	return ((ba[0] & 0xFF)) |
	       ((ba[1] & 0xFF) << 8);
}

static unsigned long long read64le(unsigned char *ba) {
	return read32le(ba) |
	       (((long long) read32le(ba + 4)) << 32);
}

static unsigned int read32be(unsigned char *ba) {
	return ((ba[0] & 0xFF) << 24) |
	       ((ba[1] & 0xFF) << 16) |
	       ((ba[2] & 0xFF) << 8) |
	       ((ba[3] & 0xFF));
}

static double toDouble(unsigned char *ba) {
	if (sizeof(double) != 8) {
		fprintf(stderr, "Internal error: wrong floating point size\n");
		exit(EXIT_FAILURE);
	}

	return *((double *) ba);
}

drawvec decode_geometry(unsigned char *data, size_t len, int *type) {
	drawvec dv;

	if (len < 4) {
		fprintf(stderr, "Geometry too short\n");
		exit(EXIT_FAILURE);
	}

	unsigned t = read32le(data);
	if (t == 1) {  // Point
		if (len < 20) {
			fprintf(stderr, "Unexpectedly short point geometry\n");
			exit(EXIT_FAILURE);
		}

		double lon = toDouble(data + 4);
		double lat = toDouble(data + 12);

		long long x, y;
		projection->project(lon, lat, 32, &x, &y);

		dv.push_back(draw(VT_MOVETO, x, y));
		*type = VT_POINT;
	} else if (t == 3) {  // MultiLineString
		unsigned parts = read32le(data + 36);
		unsigned points = read32le(data + 40);

		for (size_t i = 0; i < parts; i++) {
			unsigned start = read32le(data + 44 * 4 * i);
			unsigned end;

			if (i + 1 < parts) {
				end = read32le(data + 44 * 4 * (i + 1));
			} else {
				end = points;
			}

			for (size_t j = start; j < end; j++) {
				double lon = toDouble(data + 44 + 4 * parts + 16 * j);
				double lat = toDouble(data + 44 + 4 * parts + 16 * j + 8);

				long long x, y;
				projection->project(lon, lat, 32, &x, &y);

				if (j == start) {
					dv.push_back(draw(VT_MOVETO, x, y));
				} else {
					dv.push_back(draw(VT_LINETO, x, y));
				}
			}
		}

		*type = VT_LINE;
	} else {
		static bool warned = false;
		if (!warned) {
			fprintf(stderr, "Unsupported geometry type %u\n", t);
			*type = 0;
		}
	}

	return dv;
}

void parse_shapefile(struct serialization_state *sst, std::string fname, int layer, std::string layername) {
	std::string dbfname = fname.substr(0, fname.size() - 3) + "dbf";

	FILE *shp = fopen(fname.c_str(), "rb");
	if (shp == NULL) {
		perror(fname.c_str());
		exit(EXIT_FAILURE);
	}
	FILE *dbf = fopen(dbfname.c_str(), "rb");
	if (dbf == NULL) {
		perror(dbfname.c_str());
		exit(EXIT_FAILURE);
	}

	unsigned char shpheader[100];
	if (fread(shpheader, 1, 100, shp) != 100) {
		perror("read shapefile header");
		exit(EXIT_FAILURE);
	}

	unsigned int magic = read32be(shpheader);
	unsigned int flen = 2 * read32be(shpheader + 24) - 100;
	unsigned int version = read32le(shpheader + 28);

	if (magic != 9994 || version != 1000) {
		fprintf(stderr, "%s: not a shapefile (%u %u)\n", fname.c_str(), magic, version);
		exit(EXIT_FAILURE);
	}

	unsigned char dbfheader[32];
	if (fread(dbfheader, 1, 32, dbf) != 32) {
		perror("read dbf header");
		exit(EXIT_FAILURE);
	}

	unsigned int dbnrec = read32le(dbfheader + 4);
	unsigned int dbheaderlen = read16le(dbfheader + 8);
	unsigned int dbreclen = read16le(dbfheader + 10);

	if (dbheaderlen <= 32) {
		fprintf(stderr, "Impossible length for DBF column header %u\n", dbheaderlen);
		exit(EXIT_FAILURE);
	}

	unsigned int dbcol_len = dbheaderlen - 32;
	unsigned char dbcolumns[dbcol_len];
	if (fread(dbcolumns, 1, dbcol_len, dbf) != dbcol_len) {
		perror("read dbf column header");
		exit(EXIT_FAILURE);
	}

	std::vector<std::string> columns;
	std::vector<int> column_widths;
	std::vector<int> column_types;

	// -1 because there is a 1-byte terminator
	for (size_t i = 0; i < dbcol_len - 1; i += 32) {
		size_t j;
		for (j = i; j < i + 10; j++) {
			if (dbcolumns[j] == '\0') {
				break;
			}
		}

		columns.push_back(std::string((char *) dbcolumns + i, j - i));
		column_widths.push_back(dbcolumns[i + 16]);
		column_types.push_back(dbcolumns[i + 11]);
	}

	unsigned char db[dbreclen];
	unsigned seq = 0;
	while (fread(db, dbreclen, 1, dbf) == 1) {
		unsigned char shlen[8];
		if (fread(shlen, 8, 1, shp) != 1) {
			fprintf(stderr, "Attributes with no shape\n");
			exit(EXIT_FAILURE);
		}

		seq++;
		unsigned fileseq = read32be(shlen);
		if (fileseq != seq) {
			fprintf(stderr, "Shapefile out of sequence: found %u for record %u\n", fileseq, seq);
			exit(EXIT_FAILURE);
		}

		unsigned int geom_len = read32be(shlen + 4) * 2;
		unsigned char geom_buf[geom_len];
		if (fread(geom_buf, 1, geom_len, shp) != geom_len) {
			fprintf(stderr, "End of file reading geometry\n");
			exit(EXIT_FAILURE);
		}

		int type;
		drawvec dv = decode_geometry(geom_buf, geom_len, &type);

		if (type > 0 && dv.size() > 0) {
			serial_feature sf;

			std::vector<std::string> full_keys;
			std::vector<serial_val> full_values;

			unsigned char *dbp = db;
			for (size_t i = 0; i < columns.size(); i++) {
				std::string s = std::string((char *) dbp, column_widths[i]);
				dbp += column_widths[i];

				while (s.size() > 0 && s[s.size() - 1] == ' ') {
					s.pop_back();
				}

				if (s.size() > 0) {
					serial_val sv;
					sv.s = s;

					if (column_types[i] == 'F' || column_types[i] == 'N') {
						sv.type = mvt_double;
					} else if (column_types[i] == 'L') {
						sv.type = mvt_bool;
						if (s == "Y" || s == "y" || s == "T" || s == "t") {
							sv.s = "true";
						} else {
							sv.s = "false";
						}
					} else {
						sv.type = mvt_string;
					}

					full_keys.push_back(columns[i]);
					full_values.push_back(sv);
				}
			}

			sf.layer = layer;
			sf.layername = layername;
			sf.segment = sst->segment;
			sf.has_id = false;
			sf.id = 0;
			sf.has_tippecanoe_minzoom = false;
			sf.has_tippecanoe_maxzoom = false;
			sf.feature_minzoom = false;
			sf.seq = *(sst->layer_seq);
			sf.geometry = dv;
			sf.t = type;
			sf.full_keys = full_keys;
			sf.full_values = full_values;
			sf.m = sf.full_values.size();

			serialize_feature(sst, sf);
		}
	}

	if (seq != dbnrec) {
		fprintf(stderr, "Unexpected number of attributes: %u instead of %u\n", seq, dbnrec);
		exit(EXIT_FAILURE);
	}

	if (fclose(shp) != 0) {
		perror("fclose");
		exit(EXIT_FAILURE);
	}
	if (fclose(dbf) != 0) {
		perror("fclose");
		exit(EXIT_FAILURE);
	}
}
