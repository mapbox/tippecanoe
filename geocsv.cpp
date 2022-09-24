#include <stdlib.h>
#include <algorithm>
#include "geocsv.hpp"
#include "mvt.hpp"
#include "serial.hpp"
#include "projection.hpp"
#include "main.hpp"
#include "text.hpp"
#include "csv.hpp"
#include "milo/dtoa_milo.h"
#include "options.hpp"
#include "errors.hpp"

void parse_geocsv(std::vector<struct serialization_state> &sst, std::string fname, int layer, std::string layername) {
	FILE *f;

	if (fname.size() == 0) {
		f = stdin;
	} else {
		f = fopen(fname.c_str(), "r");
		if (f == NULL) {
			perror(fname.c_str());
			exit(EXIT_OPEN);
		}
	}

	std::string s;
	std::vector<std::string> header;
	ssize_t latcol = -1, loncol = -1;

	if ((s = csv_getline(f)).size() > 0) {
		std::string err = check_utf8(s);
		if (err != "") {
			fprintf(stderr, "%s: %s\n", fname.c_str(), err.c_str());
			exit(EXIT_UTF8);
		}

		header = csv_split(s.c_str());

		for (size_t i = 0; i < header.size(); i++) {
			header[i] = csv_dequote(header[i]);

			std::string lower(header[i]);
			std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

			if (lower == "y" || lower == "lat" || (lower.find("latitude") != std::string::npos)) {
				latcol = i;
			}
			if (lower == "x" || lower == "lon" || lower == "lng" || lower == "long" || (lower.find("longitude") != std::string::npos)) {
				loncol = i;
			}
		}
	}

	if (latcol < 0 || loncol < 0) {
		fprintf(stderr, "%s: Can't find \"lat\" and \"lon\" columns\n", fname.c_str());
		exit(EXIT_CSV);
	}

	size_t seq = 0;
	while ((s = csv_getline(f)).size() > 0) {
		std::string err = check_utf8(s);
		if (err != "") {
			fprintf(stderr, "%s: %s\n", fname.c_str(), err.c_str());
			exit(EXIT_UTF8);
		}

		seq++;
		std::vector<std::string> line = csv_split(s.c_str());

		if (line.size() != header.size()) {
			fprintf(stderr, "%s:%zu: Mismatched column count: %zu in line, %zu in header\n", fname.c_str(), seq + 1, line.size(), header.size());
			exit(EXIT_CSV);
		}

		if (line[loncol].empty() || line[latcol].empty()) {
			static int warned = 0;
			if (!warned) {
				fprintf(stderr, "%s:%zu: null geometry (additional not reported)\n", fname.c_str(), seq + 1);
				warned = 1;
			}
			continue;
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
				line[i] = csv_dequote(line[i]);

				serial_val sv;
				if (is_number(line[i])) {
					sv.type = mvt_double;
				} else if (line[i].size() == 0 && prevent[P_EMPTY_CSV_COLUMNS]) {
					sv.type = mvt_null;
					line[i] = "null";
				} else {
					sv.type = mvt_string;
				}
				sv.s = line[i];

				full_keys.push_back(header[i]);
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

		serialize_feature(&sst[0], sf);
	}

	if (fname.size() != 0) {
		if (fclose(f) != 0) {
			perror("fclose");
			exit(EXIT_CLOSE);
		}
	}
}
