#include <stdlib.h>
#include "geocsv.hpp"
#include "mvt.hpp"
#include "serial.hpp"
#include "projection.hpp"
#include "main.hpp"
#include "text.hpp"
#include "csv.hpp"
#include "milo/dtoa_milo.h"

void parse_geocsv(std::vector<struct serialization_state> &sst, std::string fname, int layer, std::string layername) {
	FILE *f = fopen(fname.c_str(), "r");
	if (f == NULL) {
		perror(fname.c_str());
		exit(EXIT_FAILURE);
	}

	std::string s;
	std::vector<std::string> header;
	ssize_t latcol = -1, loncol = -1;

	if ((s = csv_getline(f)).size() > 0) {
		header = csv_split(s.c_str());

		for (size_t i = 0; i < header.size(); i++) {
			header[i] = csv_dequote(header[i]);

			if (header[i] == "lat" || header[i] == "latitude") {
				latcol = i;
			}
			if (header[i] == "lon" || header[i] == "longitude" || header[i] == "long") {
				loncol = i;
			}
		}
	}

	if (latcol < 0 || loncol < 0) {
		fprintf(stderr, "%s: Can't find \"lat\" and \"lon\" columns\n", fname.c_str());
		exit(EXIT_FAILURE);
	}

	size_t seq = 0;
	while ((s = csv_getline(f)).size() > 0) {
		seq++;
		std::vector<std::string> line = csv_split(s.c_str());

		if (line.size() != header.size()) {
			fprintf(stderr, "%s:%zu: Mismatched column count: %zu in line, %zu in header\n", fname.c_str(), seq, line.size(), header.size());
			exit(EXIT_FAILURE);
		}

		double lon = atof(line[loncol].c_str());
		double lat = atof(line[latcol].c_str());

		long long x, y;
		projection->project(lon, lat, 32, &x, &y);
		drawvec dv;
		dv.push_back(draw(VT_MOVETO, x, y));

		std::vector<std::string> full_keys;
		std::vector<serial_val> full_values;

		for (size_t i = 0; i < line.size(); i++) {
			if (i != (size_t) latcol && i != (size_t) loncol) {
				full_keys.push_back(header[i]);

				serial_val sv;
				sv.type = mvt_string;
				sv.s = line[i];

				full_values.push_back(sv);
			}
		}

		serial_feature sf;

		sf.layer = layer;
		sf.layername = layername;
		sf.segment = sst[0].segment;
		sf.has_id = false;
		sf.id = 0;
		sf.has_tippecanoe_minzoom = false;
		sf.has_tippecanoe_maxzoom = false;
		sf.feature_minzoom = false;
		sf.seq = *(sst[0].layer_seq);
		sf.geometry = dv;
		sf.t = 1;  // POINT
		sf.full_keys = full_keys;
		sf.full_values = full_values;
		sf.m = sf.full_values.size();

		serialize_feature(&sst[0], sf);
	}

	if (fclose(f) != 0) {
		perror("fclose");
		exit(EXIT_FAILURE);
	}
}