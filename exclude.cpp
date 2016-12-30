/*
 * exclude.cpp
 *
 *  Created on: Dec 28, 2016
 *      Author: asumarli
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include <zlib.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <protozero/pbf_reader.hpp>
#include "mvt.hpp"
#include "projection.hpp"
#include "geometry.hpp"

#include "gdal.h"
#include "ogrsf_frmts.h"
#include "ogr_geometry.h"

#include "mbtiles.hpp"

struct lonlat {
	int op;
	double lon;
	double lat;
	int x;
	int y;

	lonlat(int nop, double nlon, double nlat, int nx, int ny) {
		this->op = nop;
		this->lon = nlon;
		this->lat = nlat;
		this->x = nx;
		this->y = ny;
	}
};
bool featureWithinLayer(mvt_feature &feat, int z, unsigned x, unsigned y, int extent, OGRLayer *exclude_layer, GEOSContextHandle_t geosContext) {
	OGRFeature *poFeature;
	std::vector<lonlat> ops;
	for (size_t g = 0; g < feat.geometry.size(); g++) {
		int op = feat.geometry[g].op;
		long long px = feat.geometry[g].x;
		long long py = feat.geometry[g].y;

		if (op == VT_MOVETO || op == VT_LINETO) {
			long long scale = 1LL << (32 - z);
			long long wx = scale * x + (scale / extent) * px;
			long long wy = scale * y + (scale / extent) * py;

			double lat, lon;
			projection->unproject(wx, wy, 32, &lon, &lat);

			ops.push_back(lonlat(op, lon, lat, px, py));
		} else {
			ops.push_back(lonlat(op, 0, 0, 0, 0));
		}
	}
	if(ops.size() > 0) {
		for(size_t i = 0; i < ops.size(); i++) {
			if(ops[i].lon == 0 && ops[i].lat == 0)
				continue;
			OGRPoint p(ops[i].lon, ops[i].lat);
			exclude_layer->ResetReading();

			while((poFeature = exclude_layer->GetNextFeature()) != NULL) {
				OGRGeometry *geom = poFeature->GetGeometryRef();
				if(geom->WithinEx(geosContext, &p)) {
					return true;
				}

				OGRFeature::DestroyFeature(poFeature);
				poFeature = NULL;
			}
		}
	}

	return false;
}
std::string handle(std::string message, int z, unsigned x, unsigned y, OGRLayer *exclude_layer, GEOSContextHandle_t geosContext) {
	mvt_tile tile;
	mvt_tile out;
	try {
		if (!tile.decode(message)) {
			fprintf(stderr, "Couldn't parse tile %d/%u/%u\n", z, x, y);
			exit(EXIT_FAILURE);
		}
	} catch (protozero::unknown_pbf_wire_type_exception &e) {
		fprintf(stderr, "PBF decoding error in tile %d/%u/%u\n", z, x, y);
		exit(EXIT_FAILURE);
	}

	for (size_t l = 0; l < tile.layers.size(); l++) {
		// Original Layer
		mvt_layer &layer = tile.layers[l];
		int extent = layer.extent;

		// New Layer
		mvt_layer nl;
		nl.extent = extent;
		nl.version = layer.version;
		nl.name = layer.name;
		nl.values = layer.values;
		nl.value_map = layer.value_map;
		nl.key_map = layer.key_map;
		nl.keys = layer.keys;

		for (size_t f = 0; f < layer.features.size(); f++) {
			if(!featureWithinLayer(layer.features[f], z, x, y, extent, exclude_layer, geosContext)) {
				nl.features.push_back(layer.features[f]);
			}

		}

		if(nl.features.size() > 0) {
			out.layers.push_back(nl);
		}

	} // for each layer
	return out.encode();
}

void exclude(char *in, char *out, char *shp, char **argv) {
	GDALAllRegister();

	GDALDataset       *poDS;
	poDS = (GDALDataset*) GDALOpenEx(shp, GDAL_OF_VECTOR, NULL, NULL, NULL );
	if( poDS == NULL )
	{
		fprintf(stderr, "Failed to open shapefile: %s", shp);
		exit(EXIT_FAILURE);
	}
	OGRLayer  *poLayer = poDS->GetLayer(0);
	GEOSContextHandle_t geosContext = OGRGeometry::createGEOSContext();
	sqlite3 *db;
	if (sqlite3_open(in, &db) != SQLITE_OK) {
		fprintf(stderr, "%s: %s\n", in, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
	sqlite3 *db_out = mbtiles_open(out, argv, true);
	sqlite3_stmt *stmt;
	const char *sql0 = "SELECT count(*) FROM tiles";
	if (sqlite3_prepare_v2(db, sql0, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "%s: select failed: %s\n", in, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
	sqlite3_step(stmt);
	int row_count = sqlite3_column_int(stmt, 0);

	sqlite3_finalize(stmt);
	stmt = NULL;

	const char *sql = "SELECT tile_data, zoom_level, tile_column, tile_row from tiles order by zoom_level, tile_column, tile_row;";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "%s: select failed: %s\n", in, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	int i = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int len = sqlite3_column_bytes(stmt, 0);
		int tz = sqlite3_column_int(stmt, 1);
		int tx = sqlite3_column_int(stmt, 2);
		int ty = sqlite3_column_int(stmt, 3);
		ty = (1LL << tz) - 1 - ty;
		const char *s = (const char *) sqlite3_column_blob(stmt, 0);
		std::string new_message = handle(std::string(s, len), tz, tx, ty, poLayer, geosContext);
		mbtiles_write_tile(db_out, tz, tx, ty, new_message.c_str(), new_message.length());
		printf("%c[2K", 27);
		printf("\r%g%%", (i++*100.0f)/row_count);
		fflush(stdout);
	}
	sqlite3_finalize(stmt);
	if (sqlite3_close(db) != SQLITE_OK) {
		fprintf(stderr, "%s: could not close database: %s\n", in, sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
	mbtiles_close(db_out, argv);
	OGRGeometry::freeGEOSContext(geosContext);
}
void usage(char **argv) {
	fprintf(stderr, "Usage: %s in.mbtiles out.mbtiles exclude.shp\n", argv[0]);
	exit(EXIT_FAILURE);
}
bool exists(const char *f) {
	int fd = open(f, O_RDONLY);
	if(fd >= 0) {
		close(fd);
		return true;
	}
	return false;
}
int main(int argc, char **argv) {
	if(argc != 4) {
		usage(argv);
	} else {
		char *in = argv[1];
		char *out = argv[2];
		char *shp = argv[3];

		if(exists(out)) {
			fprintf(stderr, "Output file already exists: %s", out);
			exit(EXIT_FAILURE);
		}

		if(exists(in) && exists(shp)) {
			exclude(in, out, shp, argv);
		} else {
			fprintf(stderr, "Failed to open one of the files provided...\n");
			usage(argv);
		}
	}
}
