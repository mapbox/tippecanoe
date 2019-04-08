// for vasprintf() on Linux
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <limits.h>
#include <getopt.h>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <zlib.h>
#include <math.h>
#include <pthread.h>
#include "mvt.hpp"
#include "projection.hpp"
#include "pool.hpp"
#include "mbtiles.hpp"
#include "geometry.hpp"
#include "dirtiles.hpp"
#include "evaluator.hpp"
#include "csv.hpp"
#include "text.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include "jsonpull/jsonpull.h"
#include "milo/dtoa_milo.h"

int pk = false;
int pC = false;
int pg = false;
int pe = false;
size_t CPUS;
int quiet = false;
int maxzoom = 32;
int minzoom = 0;
std::map<std::string, std::string> renames;
bool exclude_all = false;

struct stats {
	int minzoom;
	int maxzoom;
	double midlat, midlon;
	double minlat, minlon, maxlat, maxlon;
};

void aprintf(std::string *buf, const char *format, ...) {
	va_list ap;
	char *tmp;

	va_start(ap, format);
	if (vasprintf(&tmp, format, ap) < 0) {
		fprintf(stderr, "memory allocation failure\n");
		exit(EXIT_FAILURE);
	}
	va_end(ap);

	buf->append(tmp, strlen(tmp));
	free(tmp);
}

void handle(std::string message, int z, unsigned x, unsigned y, std::map<std::string, layermap_entry> &layermap, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping, std::set<std::string> &exclude, std::set<std::string> &keep_layers, std::set<std::string> &remove_layers, int ifmatched, mvt_tile &outtile, json_object *filter) {
	mvt_tile tile;
	int features_added = 0;
	bool was_compressed;

	if (!tile.decode(message, was_compressed)) {
		fprintf(stderr, "Couldn't decompress tile %d/%u/%u\n", z, x, y);
		exit(EXIT_FAILURE);
	}

	for (size_t l = 0; l < tile.layers.size(); l++) {
		mvt_layer &layer = tile.layers[l];

		auto found = renames.find(layer.name);
		if (found != renames.end()) {
			layer.name = found->second;
		}

		if (keep_layers.size() > 0 && keep_layers.count(layer.name) == 0) {
			continue;
		}
		if (remove_layers.count(layer.name) != 0) {
			continue;
		}

		size_t ol;
		for (ol = 0; ol < outtile.layers.size(); ol++) {
			if (tile.layers[l].name == outtile.layers[ol].name) {
				break;
			}
		}

		if (ol == outtile.layers.size()) {
			outtile.layers.push_back(mvt_layer());

			outtile.layers[ol].name = layer.name;
			outtile.layers[ol].version = layer.version;
			outtile.layers[ol].extent = layer.extent;
		}

		mvt_layer &outlayer = outtile.layers[ol];

		if (layer.extent != outlayer.extent) {
			if (layer.extent > outlayer.extent) {
				for (size_t i = 0; i < outlayer.features.size(); i++) {
					for (size_t j = 0; j < outlayer.features[i].geometry.size(); j++) {
						outlayer.features[i].geometry[j].x = outlayer.features[i].geometry[j].x * layer.extent / outlayer.extent;
						outlayer.features[i].geometry[j].y = outlayer.features[i].geometry[j].y * layer.extent / outlayer.extent;
					}
				}

				outlayer.extent = layer.extent;
			}
		}

		auto file_keys = layermap.find(layer.name);

		for (size_t f = 0; f < layer.features.size(); f++) {
			mvt_feature feat = layer.features[f];
			std::set<std::string> exclude_attributes;

			if (filter != NULL) {
				std::map<std::string, mvt_value> attributes;

				for (size_t t = 0; t + 1 < feat.tags.size(); t += 2) {
					std::string key = layer.keys[feat.tags[t]];
					mvt_value &val = layer.values[feat.tags[t + 1]];

					attributes.insert(std::pair<std::string, mvt_value>(key, val));
				}

				if (feat.has_id) {
					mvt_value v;
					v.type = mvt_uint;
					v.numeric_value.uint_value = feat.id;

					attributes.insert(std::pair<std::string, mvt_value>("$id", v));
				}

				mvt_value v;
				v.type = mvt_string;

				if (feat.type == mvt_point) {
					v.string_value = "Point";
				} else if (feat.type == mvt_linestring) {
					v.string_value = "LineString";
				} else if (feat.type == mvt_polygon) {
					v.string_value = "Polygon";
				}

				attributes.insert(std::pair<std::string, mvt_value>("$type", v));

				mvt_value v2;
				v2.type = mvt_uint;
				v2.numeric_value.uint_value = z;

				attributes.insert(std::pair<std::string, mvt_value>("$zoom", v2));

				if (!evaluate(attributes, layer.name, filter, exclude_attributes)) {
					continue;
				}
			}

			mvt_feature outfeature;
			int matched = 0;

			if (feat.has_id) {
				outfeature.has_id = true;
				outfeature.id = feat.id;
			}

			std::map<std::string, std::pair<mvt_value, type_and_string>> attributes;
			std::vector<std::string> key_order;

			for (size_t t = 0; t + 1 < feat.tags.size(); t += 2) {
				const char *key = layer.keys[feat.tags[t]].c_str();
				mvt_value &val = layer.values[feat.tags[t + 1]];
				std::string value;
				int type = -1;

				if (val.type == mvt_string) {
					value = val.string_value;
					type = mvt_string;
				} else if (val.type == mvt_int) {
					aprintf(&value, "%lld", (long long) val.numeric_value.int_value);
					type = mvt_double;
				} else if (val.type == mvt_double) {
					aprintf(&value, "%s", milo::dtoa_milo(val.numeric_value.double_value).c_str());
					type = mvt_double;
				} else if (val.type == mvt_float) {
					aprintf(&value, "%s", milo::dtoa_milo(val.numeric_value.float_value).c_str());
					type = mvt_double;
				} else if (val.type == mvt_bool) {
					aprintf(&value, "%s", val.numeric_value.bool_value ? "true" : "false");
					type = mvt_bool;
				} else if (val.type == mvt_sint) {
					aprintf(&value, "%lld", (long long) val.numeric_value.sint_value);
					type = mvt_double;
				} else if (val.type == mvt_uint) {
					aprintf(&value, "%llu", (long long) val.numeric_value.uint_value);
					type = mvt_double;
				} else {
					continue;
				}

				if (type < 0) {
					continue;
				}

				if (!exclude_all && exclude.count(std::string(key)) == 0 && exclude_attributes.count(std::string(key)) == 0) {
					type_and_string tas;
					tas.type = type;
					tas.string = value;

					attributes.insert(std::pair<std::string, std::pair<mvt_value, type_and_string>>(key, std::pair<mvt_value, type_and_string>(val, tas)));
					key_order.push_back(key);
				}

				if (header.size() > 0 && strcmp(key, header[0].c_str()) == 0) {
					std::map<std::string, std::vector<std::string>>::iterator ii = mapping.find(value);

					if (ii != mapping.end()) {
						std::vector<std::string> fields = ii->second;
						matched = 1;

						for (size_t i = 1; i < fields.size(); i++) {
							std::string joinkey = header[i];
							std::string joinval = fields[i];
							int attr_type = mvt_string;

							if (joinval.size() > 0) {
								if (joinval[0] == '"') {
									joinval = csv_dequote(joinval);
								} else if (is_number(joinval)) {
									attr_type = mvt_double;
								}
							} else if (pe) {
								attr_type = mvt_null;
							}

							const char *sjoinkey = joinkey.c_str();

							if (!exclude_all && exclude.count(joinkey) == 0 && exclude_attributes.count(joinkey) == 0 && attr_type != mvt_null) {
								mvt_value outval;
								if (attr_type == mvt_string) {
									outval.type = mvt_string;
									outval.string_value = joinval;
								} else {
									outval.type = mvt_double;
									outval.numeric_value.double_value = atof(joinval.c_str());
								}

								auto fa = attributes.find(sjoinkey);
								if (fa != attributes.end()) {
									attributes.erase(fa);
								}

								type_and_string tas;
								tas.type = outval.type;
								tas.string = joinval;

								// Convert from double to int if the joined attribute is an integer
								outval = stringified_to_mvt_value(outval.type, joinval.c_str());

								attributes.insert(std::pair<std::string, std::pair<mvt_value, type_and_string>>(joinkey, std::pair<mvt_value, type_and_string>(outval, tas)));
								key_order.push_back(joinkey);
							}
						}
					}
				}
			}

			if (matched || !ifmatched) {
				if (file_keys == layermap.end()) {
					layermap.insert(std::pair<std::string, layermap_entry>(layer.name, layermap_entry(layermap.size())));
					file_keys = layermap.find(layer.name);
					file_keys->second.minzoom = z;
					file_keys->second.maxzoom = z;
				}

				// To keep attributes in their original order instead of alphabetical
				for (auto k : key_order) {
					auto fa = attributes.find(k);

					if (fa != attributes.end()) {
						outlayer.tag(outfeature, k, fa->second.first);
						add_to_file_keys(file_keys->second.file_keys, k, fa->second.second);
						attributes.erase(fa);
					}
				}

				outfeature.type = feat.type;
				outfeature.geometry = feat.geometry;

				if (layer.extent != outlayer.extent) {
					for (size_t i = 0; i < outfeature.geometry.size(); i++) {
						outfeature.geometry[i].x = outfeature.geometry[i].x * outlayer.extent / layer.extent;
						outfeature.geometry[i].y = outfeature.geometry[i].y * outlayer.extent / layer.extent;
					}
				}

				features_added++;
				outlayer.features.push_back(outfeature);

				if (z < file_keys->second.minzoom) {
					file_keys->second.minzoom = z;
				}
				if (z > file_keys->second.maxzoom) {
					file_keys->second.maxzoom = z;
				}

				if (feat.type == mvt_point) {
					file_keys->second.points++;
				} else if (feat.type == mvt_linestring) {
					file_keys->second.lines++;
				} else if (feat.type == mvt_polygon) {
					file_keys->second.polygons++;
				}
			}
		}
	}

	if (features_added == 0) {
		return;
	}
}

double min(double a, double b) {
	if (a < b) {
		return a;
	} else {
		return b;
	}
}

double max(double a, double b) {
	if (a > b) {
		return a;
	} else {
		return b;
	}
}

struct reader {
	long long zoom = 0;
	long long x = 0;
	long long sorty = 0;
	long long y = 0;
	int z_flag = 0;

	std::string data = "";

	std::vector<zxy> dirtiles;
	std::string dirbase;
	std::string name;

	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;
	struct reader *next = NULL;

	bool operator<(const struct reader &r) const {
		if (zoom < r.zoom) {
			return true;
		}
		if (zoom > r.zoom) {
			return false;
		}

		if (x < r.x) {
			return true;
		}
		if (x > r.x) {
			return false;
		}

		if (sorty < r.sorty) {
			return true;
		}
		if (sorty > r.sorty) {
			return false;
		}

		if (data < r.data) {
			return true;
		}

		return false;
	}
};

struct reader *begin_reading(char *fname) {
	struct reader *r = new reader;
	r->name = fname;

	struct stat st;
	if (stat(fname, &st) == 0 && (st.st_mode & S_IFDIR) != 0) {
		r->db = NULL;
		r->stmt = NULL;
		r->next = NULL;

		r->dirtiles = enumerate_dirtiles(fname, minzoom, maxzoom);
		r->dirbase = fname;

		if (r->dirtiles.size() == 0) {
			r->zoom = 32;
		} else {
			r->zoom = r->dirtiles[0].z;
			r->x = r->dirtiles[0].x;
			r->y = r->dirtiles[0].y;
			r->sorty = (1LL << r->zoom) - 1 - r->y;
			r->data = dir_read_tile(r->dirbase, r->dirtiles[0]);

			r->dirtiles.erase(r->dirtiles.begin());
		}
	} else {
		sqlite3 *db;

		if (sqlite3_open(fname, &db) != SQLITE_OK) {
			fprintf(stderr, "%s: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}

		char *err = NULL;
		if (sqlite3_exec(db, "PRAGMA integrity_check;", NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "%s: integrity_check: %s\n", fname, err);
			exit(EXIT_FAILURE);
		}

		const char *sql = "SELECT zoom_level, tile_column, tile_row, tile_data from tiles order by zoom_level, tile_column, tile_row;";
		sqlite3_stmt *stmt;

		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
			fprintf(stderr, "%s: select failed: %s\n", fname, sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}

		r->db = db;
		r->stmt = stmt;
		r->next = NULL;

		if (sqlite3_step(stmt) == SQLITE_ROW) {
			r->zoom = sqlite3_column_int(stmt, 0);
			r->x = sqlite3_column_int(stmt, 1);
			r->sorty = sqlite3_column_int(stmt, 2);
			r->y = (1LL << r->zoom) - 1 - r->sorty;

			const char *data = (const char *) sqlite3_column_blob(stmt, 3);
			size_t len = sqlite3_column_bytes(stmt, 3);

			r->data = std::string(data, len);
		} else {
			r->zoom = 32;
		}
	}

	return r;
}

struct arg {
	std::map<zxy, std::vector<std::string>> inputs{};
	std::map<zxy, std::string> outputs{};

	std::map<std::string, layermap_entry> *layermap = NULL;

	std::vector<std::string> *header = NULL;
	std::map<std::string, std::vector<std::string>> *mapping = NULL;
	std::set<std::string> *exclude = NULL;
	std::set<std::string> *keep_layers = NULL;
	std::set<std::string> *remove_layers = NULL;
	int ifmatched = 0;
	json_object *filter = NULL;
};

void *join_worker(void *v) {
	arg *a = (arg *) v;

	for (auto ai = a->inputs.begin(); ai != a->inputs.end(); ++ai) {
		mvt_tile tile;

		for (size_t i = 0; i < ai->second.size(); i++) {
			handle(ai->second[i], ai->first.z, ai->first.x, ai->first.y, *(a->layermap), *(a->header), *(a->mapping), *(a->exclude), *(a->keep_layers), *(a->remove_layers), a->ifmatched, tile, a->filter);
		}

		ai->second.clear();

		bool anything = false;
		mvt_tile outtile;
		for (size_t i = 0; i < tile.layers.size(); i++) {
			if (tile.layers[i].features.size() > 0) {
				outtile.layers.push_back(tile.layers[i]);
				anything = true;
			}
		}

		if (anything) {
			std::string pbf = outtile.encode();
			std::string compressed;

			if (!pC) {
				compress(pbf, compressed);
			} else {
				compressed = pbf;
			}

			if (!pk && compressed.size() > 500000) {
				fprintf(stderr, "Tile %lld/%lld/%lld size is %lld, >500000. Skipping this tile\n.", ai->first.z, ai->first.x, ai->first.y, (long long) compressed.size());
			} else {
				a->outputs.insert(std::pair<zxy, std::string>(ai->first, compressed));
			}
		}
	}

	return NULL;
}

void handle_tasks(std::map<zxy, std::vector<std::string>> &tasks, std::vector<std::map<std::string, layermap_entry>> &layermaps, sqlite3 *outdb, const char *outdir, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping, std::set<std::string> &exclude, int ifmatched, std::set<std::string> &keep_layers, std::set<std::string> &remove_layers, json_object *filter) {
	pthread_t pthreads[CPUS];
	std::vector<arg> args;

	for (size_t i = 0; i < CPUS; i++) {
		args.push_back(arg());

		args[i].layermap = &layermaps[i];
		args[i].header = &header;
		args[i].mapping = &mapping;
		args[i].exclude = &exclude;
		args[i].keep_layers = &keep_layers;
		args[i].remove_layers = &remove_layers;
		args[i].ifmatched = ifmatched;
		args[i].filter = filter;
	}

	size_t count = 0;
	// This isn't careful about distributing tasks evenly across CPUs,
	// but, from testing, it actually takes a little longer to do
	// the proper allocation than is saved by perfectly balanced threads.
	for (auto ai = tasks.begin(); ai != tasks.end(); ++ai) {
		args[count].inputs.insert(*ai);
		count = (count + 1) % CPUS;

		if (ai == tasks.begin()) {
			if (!quiet) {
				fprintf(stderr, "%lld/%lld/%lld  \r", ai->first.z, ai->first.x, ai->first.y);
			}
		}
	}

	for (size_t i = 0; i < CPUS; i++) {
		if (pthread_create(&pthreads[i], NULL, join_worker, &args[i]) != 0) {
			perror("pthread_create");
			exit(EXIT_FAILURE);
		}
	}

	for (size_t i = 0; i < CPUS; i++) {
		void *retval;

		if (pthread_join(pthreads[i], &retval) != 0) {
			perror("pthread_join");
		}

		for (auto ai = args[i].outputs.begin(); ai != args[i].outputs.end(); ++ai) {
			if (outdb != NULL) {
				mbtiles_write_tile(outdb, ai->first.z, ai->first.x, ai->first.y, ai->second.data(), ai->second.size());
			} else if (outdir != NULL) {
				dir_write_tile(outdir, ai->first.z, ai->first.x, ai->first.y, ai->second);
			}
		}
	}
}

void handle_vector_layers(json_object *vector_layers, std::map<std::string, layermap_entry> &layermap, std::map<std::string, std::string> &attribute_descriptions) {
	if (vector_layers != NULL && vector_layers->type == JSON_ARRAY) {
		for (size_t i = 0; i < vector_layers->length; i++) {
			if (vector_layers->array[i]->type == JSON_HASH) {
				json_object *id = json_hash_get(vector_layers->array[i], "id");
				json_object *desc = json_hash_get(vector_layers->array[i], "description");

				if (id != NULL && desc != NULL && id->type == JSON_STRING && desc->type == JSON_STRING) {
					std::string sid = id->string;
					std::string sdesc = desc->string;

					if (sdesc.size() != 0) {
						auto f = layermap.find(sid);
						if (f != layermap.end()) {
							f->second.description = sdesc;
						}
					}
				}

				json_object *fields = json_hash_get(vector_layers->array[i], "fields");
				if (fields != NULL && fields->type == JSON_HASH) {
					for (size_t j = 0; j < fields->length; j++) {
						if (fields->keys[j]->type == JSON_STRING && fields->values[j]->type) {
							const char *desc2 = fields->values[j]->string;

							if (strcmp(desc2, "Number") != 0 &&
							    strcmp(desc2, "String") != 0 &&
							    strcmp(desc2, "Boolean") != 0 &&
							    strcmp(desc2, "Mixed") != 0) {
								attribute_descriptions.insert(std::pair<std::string, std::string>(fields->keys[j]->string, desc2));
							}
						}
					}
				}
			}
		}
	}
}

void decode(struct reader *readers, std::map<std::string, layermap_entry> &layermap, sqlite3 *outdb, const char *outdir, struct stats *st, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping, std::set<std::string> &exclude, int ifmatched, std::string &attribution, std::string &description, std::set<std::string> &keep_layers, std::set<std::string> &remove_layers, std::string &name, json_object *filter, std::map<std::string, std::string> &attribute_descriptions, std::string &generator_options) {
	std::vector<std::map<std::string, layermap_entry>> layermaps;
	for (size_t i = 0; i < CPUS; i++) {
		layermaps.push_back(std::map<std::string, layermap_entry>());
	}

	std::map<zxy, std::vector<std::string>> tasks;
	double minlat = INT_MAX;
	double minlon = INT_MAX;
	double maxlat = INT_MIN;
	double maxlon = INT_MIN;
	int zoom_for_bbox = -1;

	while (readers != NULL && readers->zoom < 32) {
		reader *r = readers;
		readers = readers->next;
		r->next = NULL;
		if (r->zoom != zoom_for_bbox) {
			// Only use highest zoom for bbox calculation
			// to avoid z0 always covering the world

			minlat = minlon = INT_MAX;
			maxlat = maxlon = INT_MIN;
			zoom_for_bbox = r->zoom;
		}

		double lat1, lon1, lat2, lon2;
		tile2lonlat(r->x, r->y, r->zoom, &lon1, &lat1);
		tile2lonlat(r->x + 1, r->y + 1, r->zoom, &lon2, &lat2);
		minlat = min(lat2, minlat);
		minlon = min(lon1, minlon);
		maxlat = max(lat1, maxlat);
		maxlon = max(lon2, maxlon);

		if (r->zoom >= minzoom && r->zoom <= maxzoom) {
			zxy tile = zxy(r->zoom, r->x, r->y);
			if (tasks.count(tile) == 0) {
				tasks.insert(std::pair<zxy, std::vector<std::string>>(tile, std::vector<std::string>()));
			}
			auto f = tasks.find(tile);
			f->second.push_back(r->data);
		}

		if (readers == NULL || readers->zoom != r->zoom || readers->x != r->x || readers->y != r->y) {
			if (tasks.size() > 100 * CPUS) {
				handle_tasks(tasks, layermaps, outdb, outdir, header, mapping, exclude, ifmatched, keep_layers, remove_layers, filter);
				tasks.clear();
			}
		}

		if (r->db != NULL) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				r->zoom = sqlite3_column_int(r->stmt, 0);
				r->x = sqlite3_column_int(r->stmt, 1);
				r->sorty = sqlite3_column_int(r->stmt, 2);
				r->y = (1LL << r->zoom) - 1 - r->sorty;
				const char *data = (const char *) sqlite3_column_blob(r->stmt, 3);
				size_t len = sqlite3_column_bytes(r->stmt, 3);

				r->data = std::string(data, len);
			} else {
				r->zoom = 32;
			}
		} else {
			if (r->dirtiles.size() == 0) {
				r->zoom = 32;
			} else {
				r->zoom = r->dirtiles[0].z;
				r->x = r->dirtiles[0].x;
				r->y = r->dirtiles[0].y;
				r->sorty = (1LL << r->zoom) - 1 - r->y;
				r->data = dir_read_tile(r->dirbase, r->dirtiles[0]);

				r->dirtiles.erase(r->dirtiles.begin());
			}
		}

		struct reader **rr;

		for (rr = &readers; *rr != NULL; rr = &((*rr)->next)) {
			if (*r < **rr) {
				break;
			}
		}

		r->next = *rr;
		*rr = r;
	}

	st->minlon = min(minlon, st->minlon);
	st->maxlon = max(maxlon, st->maxlon);
	st->minlat = min(minlat, st->minlat);
	st->maxlat = max(maxlat, st->maxlat);

	handle_tasks(tasks, layermaps, outdb, outdir, header, mapping, exclude, ifmatched, keep_layers, remove_layers, filter);
	layermap = merge_layermaps(layermaps);

	struct reader *next;
	for (struct reader *r = readers; r != NULL; r = next) {
		next = r->next;

		sqlite3 *db = r->db;
		if (db == NULL) {
			db = dirmeta2tmp(r->dirbase.c_str());
		} else {
			sqlite3_finalize(r->stmt);
		}

		if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'minzoom'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				int minz = max(sqlite3_column_int(r->stmt, 0), minzoom);
				st->minzoom = min(st->minzoom, minz);
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'maxzoom'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				int maxz = min(sqlite3_column_int(r->stmt, 0), maxzoom);

				if (st->maxzoom >= 0 && maxz != st->maxzoom) {
					fprintf(stderr, "Warning: mismatched maxzooms: %d in %s vs previous %d\n", maxz, r->name.c_str(), st->maxzoom);
				}

				st->maxzoom = max(st->maxzoom, maxz);
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'center'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				const unsigned char *s = sqlite3_column_text(r->stmt, 0);
				if (s != NULL) {
					sscanf((char *) s, "%lf,%lf", &st->midlon, &st->midlat);
				}
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'attribution'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				const unsigned char *s = sqlite3_column_text(r->stmt, 0);
				if (s != NULL) {
					attribution = std::string((char *) s);
				}
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'description'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				const unsigned char *s = sqlite3_column_text(r->stmt, 0);
				if (s != NULL) {
					description = std::string((char *) s);
				}
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'name'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				const unsigned char *s = sqlite3_column_text(r->stmt, 0);
				if (s != NULL) {
					if (name.size() == 0) {
						name = std::string((char *) s);
					} else {
						std::string proposed = name + " + " + std::string((char *) s);
						if (proposed.size() < 255) {
							name = proposed;
						}
					}
				}
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'bounds'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				const unsigned char *s = sqlite3_column_text(r->stmt, 0);
				if (s != NULL) {
					if (sscanf((char *) s, "%lf,%lf,%lf,%lf", &minlon, &minlat, &maxlon, &maxlat) == 4) {
						st->minlon = min(minlon, st->minlon);
						st->maxlon = max(maxlon, st->maxlon);
						st->minlat = min(minlat, st->minlat);
						st->maxlat = max(maxlat, st->maxlat);
					}
				}
			}
			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'json'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				const unsigned char *s = sqlite3_column_text(r->stmt, 0);

				if (s != NULL) {
					json_pull *jp = json_begin_string((const char *) s);
					json_object *o = json_read_tree(jp);

					if (o != NULL && o->type == JSON_HASH) {
						json_object *vector_layers = json_hash_get(o, "vector_layers");

						handle_vector_layers(vector_layers, layermap, attribute_descriptions);
						json_free(o);
					}

					json_end(jp);
				}
			}

			sqlite3_finalize(r->stmt);
		}
		if (sqlite3_prepare_v2(db, "SELECT value from metadata where name = 'generator_options'", -1, &r->stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(r->stmt) == SQLITE_ROW) {
				const unsigned char *s = sqlite3_column_text(r->stmt, 0);
				if (s != NULL) {
					if (generator_options.size() != 0) {
						generator_options.append("; ");
						generator_options.append((const char *) s);
					} else {
						generator_options = (const char *) s;
					}
				}
			}
			sqlite3_finalize(r->stmt);
		}

		// Closes either real db or temp mirror of metadata.json
		if (sqlite3_close(db) != SQLITE_OK) {
			fprintf(stderr, "Could not close database: %s\n", sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}

		delete r;
	}
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s [-f] [-i] [-pk] [-pC] [-c joins.csv] [-X] [-x exclude ...] -o new.mbtiles source.mbtiles ...\n", argv[0]);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	char *out_mbtiles = NULL;
	char *out_dir = NULL;
	sqlite3 *outdb = NULL;
	char *csv = NULL;
	int force = 0;
	int ifmatched = 0;
	json_object *filter = NULL;

	CPUS = sysconf(_SC_NPROCESSORS_ONLN);

	const char *TIPPECANOE_MAX_THREADS = getenv("TIPPECANOE_MAX_THREADS");
	if (TIPPECANOE_MAX_THREADS != NULL) {
		CPUS = atoi(TIPPECANOE_MAX_THREADS);
	}
	if (CPUS < 1) {
		CPUS = 1;
	}

	std::vector<std::string> header;
	std::map<std::string, std::vector<std::string>> mapping;

	std::set<std::string> exclude;
	std::set<std::string> keep_layers;
	std::set<std::string> remove_layers;

	std::string set_name, set_description, set_attribution;

	struct option long_options[] = {
		{"output", required_argument, 0, 'o'},
		{"output-to-directory", required_argument, 0, 'e'},
		{"force", no_argument, 0, 'f'},
		{"if-matched", no_argument, 0, 'i'},
		{"attribution", required_argument, 0, 'A'},
		{"name", required_argument, 0, 'n'},
		{"description", required_argument, 0, 'N'},
		{"prevent", required_argument, 0, 'p'},
		{"csv", required_argument, 0, 'c'},
		{"exclude", required_argument, 0, 'x'},
		{"exclude-all", no_argument, 0, 'X'},
		{"layer", required_argument, 0, 'l'},
		{"exclude-layer", required_argument, 0, 'L'},
		{"quiet", no_argument, 0, 'q'},
		{"maximum-zoom", required_argument, 0, 'z'},
		{"minimum-zoom", required_argument, 0, 'Z'},
		{"feature-filter-file", required_argument, 0, 'J'},
		{"feature-filter", required_argument, 0, 'j'},
		{"rename-layer", required_argument, 0, 'R'},

		{"no-tile-size-limit", no_argument, &pk, 1},
		{"no-tile-compression", no_argument, &pC, 1},
		{"empty-csv-columns-are-null", no_argument, &pe, 1},
		{"no-tile-stats", no_argument, &pg, 1},

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

	extern int optind;
	extern char *optarg;
	int i;

	std::string commandline = format_commandline(argc, argv);

	while ((i = getopt_long(argc, argv, getopt_str.c_str(), long_options, NULL)) != -1) {
		switch (i) {
		case 0:
			break;

		case 'o':
			out_mbtiles = optarg;
			break;

		case 'e':
			out_dir = optarg;
			break;

		case 'f':
			force = 1;
			break;

		case 'i':
			ifmatched = 1;
			break;

		case 'A':
			set_attribution = optarg;
			break;

		case 'n':
			set_name = optarg;
			break;

		case 'N':
			set_description = optarg;
			break;

		case 'z':
			maxzoom = atoi(optarg);
			break;

		case 'Z':
			minzoom = atoi(optarg);
			break;

		case 'J':
			filter = read_filter(optarg);
			break;

		case 'j':
			filter = parse_filter(optarg);
			break;

		case 'p':
			if (strcmp(optarg, "k") == 0) {
				pk = true;
			} else if (strcmp(optarg, "C") == 0) {
				pC = true;
			} else if (strcmp(optarg, "g") == 0) {
				pg = true;
			} else if (strcmp(optarg, "e") == 0) {
				pe = true;
			} else {
				fprintf(stderr, "%s: Unknown option for -p%s\n", argv[0], optarg);
				exit(EXIT_FAILURE);
			}
			break;

		case 'c':
			if (csv != NULL) {
				fprintf(stderr, "Only one -c for now\n");
				exit(EXIT_FAILURE);
			}

			csv = optarg;
			readcsv(csv, header, mapping);
			break;

		case 'x':
			exclude.insert(std::string(optarg));
			break;

		case 'X':
			exclude_all = true;
			break;

		case 'l':
			keep_layers.insert(std::string(optarg));
			break;

		case 'L':
			remove_layers.insert(std::string(optarg));
			break;

		case 'R': {
			char *cp = strchr(optarg, ':');
			if (cp == NULL || cp == optarg) {
				fprintf(stderr, "%s: -R requires old:new\n", argv[0]);
				exit(EXIT_FAILURE);
			}
			std::string before = std::string(optarg).substr(0, cp - optarg);
			std::string after = std::string(cp + 1);
			renames.insert(std::pair<std::string, std::string>(before, after));
			break;
		}

		case 'q':
			quiet = true;
			break;

		default:
			usage(argv);
		}
	}

	if (argc - optind < 1) {
		usage(argv);
	}

	if (out_mbtiles == NULL && out_dir == NULL) {
		fprintf(stderr, "%s: must specify -o out.mbtiles or -e directory\n", argv[0]);
		usage(argv);
	}

	if (out_mbtiles != NULL && out_dir != NULL) {
		fprintf(stderr, "%s: Options -o and -e cannot be used together\n", argv[0]);
		usage(argv);
	}

	if (minzoom > maxzoom) {
		fprintf(stderr, "%s: Minimum zoom -Z%d cannot be greater than maxzoom -z%d\n", argv[0], minzoom, maxzoom);
		exit(EXIT_FAILURE);
	}

	if (out_mbtiles != NULL) {
		if (force) {
			unlink(out_mbtiles);
		}
		outdb = mbtiles_open(out_mbtiles, argv, 0);
	}
	if (out_dir != NULL) {
		check_dir(out_dir, argv, force, false);
	}

	struct stats st;
	memset(&st, 0, sizeof(st));
	st.minzoom = st.minlat = st.minlon = INT_MAX;
	st.maxzoom = st.maxlat = st.maxlon = INT_MIN;

	std::map<std::string, layermap_entry> layermap;
	std::string attribution;
	std::string description;
	std::string name;

	struct reader *readers = NULL;

	for (i = optind; i < argc; i++) {
		reader *r = begin_reading(argv[i]);
		struct reader **rr;

		for (rr = &readers; *rr != NULL; rr = &((*rr)->next)) {
			if (*r < **rr) {
				break;
			}
		}

		r->next = *rr;
		*rr = r;
	}

	std::map<std::string, std::string> attribute_descriptions;
	std::string generator_options;

	decode(readers, layermap, outdb, out_dir, &st, header, mapping, exclude, ifmatched, attribution, description, keep_layers, remove_layers, name, filter, attribute_descriptions, generator_options);

	if (set_attribution.size() != 0) {
		attribution = set_attribution;
	}
	if (set_description.size() != 0) {
		description = set_description;
	}
	if (set_name.size() != 0) {
		name = set_name;
	}

	if (generator_options.size() != 0) {
		generator_options.append("; ");
	}
	generator_options.append(commandline);

	for (auto &l : layermap) {
		if (l.second.minzoom < st.minzoom) {
			st.minzoom = l.second.minzoom;
		}
		if (l.second.maxzoom > st.maxzoom) {
			st.maxzoom = l.second.maxzoom;
		}
	}

	mbtiles_write_metadata(outdb, out_dir, name.c_str(), st.minzoom, st.maxzoom, st.minlat, st.minlon, st.maxlat, st.maxlon, st.midlat, st.midlon, 0, attribution.size() != 0 ? attribution.c_str() : NULL, layermap, true, description.c_str(), !pg, attribute_descriptions, "tile-join", generator_options);

	if (outdb != NULL) {
		mbtiles_close(outdb, argv[0]);
	}

	if (filter != NULL) {
		json_free(filter);
	}

	return 0;
}
