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
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>

extern "C" {
#include "jsonpull/jsonpull.h"
}

std::string dequote(std::string s);

bool pk = false;
bool pC = false;
size_t CPUS;

struct stats {
	int minzoom;
	int maxzoom;
	double midlat, midlon;
	double minlat, minlon, maxlat, maxlon;
};

void handle(std::string message, int z, unsigned x, unsigned y, std::map<std::string, layermap_entry> &layermap, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping, std::set<std::string> &exclude, std::set<std::string> &keep_layers, std::set<std::string> &remove_layers, int ifmatched, mvt_tile &outtile) {
	mvt_tile tile;
	int features_added = 0;
	bool was_compressed;

	if (!tile.decode(message, was_compressed)) {
		fprintf(stderr, "Couldn't decompress tile %d/%u/%u\n", z, x, y);
		exit(EXIT_FAILURE);
	}

	for (size_t l = 0; l < tile.layers.size(); l++) {
		mvt_layer &layer = tile.layers[l];

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

		if (layermap.count(layer.name) == 0) {
			layermap.insert(std::pair<std::string, layermap_entry>(layer.name, layermap_entry(layermap.size())));
			auto file_keys = layermap.find(layer.name);
			file_keys->second.minzoom = z;
			file_keys->second.maxzoom = z;
		}
		auto file_keys = layermap.find(layer.name);

		for (size_t f = 0; f < layer.features.size(); f++) {
			mvt_feature feat = layer.features[f];
			mvt_feature outfeature;
			int matched = 0;

			if (feat.has_id) {
				outfeature.has_id = true;
				outfeature.id = feat.id;
			}

			std::map<std::string, mvt_value> attributes;
			std::map<std::string, int> types;
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
					aprintf(&value, "%g", val.numeric_value.double_value);
					type = mvt_double;
				} else if (val.type == mvt_float) {
					aprintf(&value, "%g", val.numeric_value.float_value);
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

				if (exclude.count(std::string(key)) == 0) {
					attributes.insert(std::pair<std::string, mvt_value>(key, val));
					types.insert(std::pair<std::string, int>(key, type));
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
									joinval = dequote(joinval);
								} else if ((joinval[0] >= '0' && joinval[0] <= '9') || joinval[0] == '-') {
									attr_type = mvt_double;
								}
							}

							const char *sjoinkey = joinkey.c_str();

							if (exclude.count(joinkey) == 0) {
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
								auto ft = types.find(sjoinkey);
								if (ft != types.end()) {
									types.erase(ft);
								}

								attributes.insert(std::pair<std::string, mvt_value>(sjoinkey, outval));
								types.insert(std::pair<std::string, int>(sjoinkey, attr_type));
								key_order.push_back(sjoinkey);
							}
						}
					}
				}
			}

			for (auto tp : types) {
				type_and_string tas;
				tas.string = tp.first;
				tas.type = tp.second;

				file_keys->second.file_keys.insert(tas);
			}

			// To keep attributes in their original order instead of alphabetical
			for (auto k : key_order) {
				auto fa = attributes.find(k);

				if (fa != attributes.end()) {
					outlayer.tag(outfeature, k, fa->second);
					attributes.erase(fa);
				}
			}

			if (matched || !ifmatched) {
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
	long long zoom;
	long long x;
	long long sorty;
	long long y;
	int pbf_count;
	int z_flag;

	std::string data;
	std::vector<std::string> pbf_path;
	std::vector<std::string> large_zoom;

	sqlite3 *db;
	sqlite3_stmt *stmt;
	struct reader *next;

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

std::vector<std::string> split_slash(std::string pbf_path) {
	std::vector<std::string> path_parts;
	std::string path(pbf_path);
	std::istringstream iss(path);
	std::string token;

	while (std::getline(iss, token, '/')) {
		path_parts.push_back(token);
	}

	return path_parts;
}

int filter(const struct dirent *dir) {
	if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0 || strcmp(dir->d_name, ".DS_Store") == 0 || strcmp(dir->d_name, "metadata.json") == 0) {
		return 0;
	} else {
		return 1;
	}
}

// Recursively walk through a specified directory and its subdirectories,
// using alphasort function and integer variable zoom_range to handle input in numerical order.
// Store the path of all pbf files in a pbf_path vector member of reader struct,
// with the help of a large_zoom vector and two integer members pbf_count and z_flag
// to ensure the tiles order in pbf_path to be the same as in mbtiles.
struct reader *read_dir(struct reader *readers, const char *name, int level, int zoom_range) {
	struct dirent **namelist;
	struct stat buf;
	std::string path;
	int i = 0;
	int n = scandir(name, &namelist, filter, alphasort);
	std::vector<std::string> path_parts1, path_parts2;
	readers->pbf_count = 0;

	if (n > 0) {
		while (i < n) {
			path = std::string(name) + "/" + std::string(namelist[i]->d_name);

			if (stat(path.c_str(), &buf) == 0 && S_ISDIR(buf.st_mode)) {
				if (level == 0) {
					if (std::stoi(namelist[i]->d_name) <= 9) {
						zoom_range = 0;
					} else {
						zoom_range = 1;
					}

					if (readers->pbf_count > 0) {
						if (readers->z_flag == 0) {
							std::sort(readers->pbf_path.end() - (readers->pbf_count + 1), readers->pbf_path.end(), std::greater<std::string>());
						} else {
							std::sort(readers->large_zoom.end() - (readers->pbf_count + 1), readers->large_zoom.end(), std::greater<std::string>());
						}
						readers->pbf_count = 0;
					}
				}

				if (level == 1 && readers->pbf_count > 0) {
					if (zoom_range == 0) {
						std::sort(readers->pbf_path.end() - (readers->pbf_count + 1), readers->pbf_path.end(), std::greater<std::string>());
					} else {
						std::sort(readers->large_zoom.end() - (readers->pbf_count + 1), readers->large_zoom.end(), std::greater<std::string>());
					}
					readers->pbf_count = 0;
				}

				read_dir(readers, path.c_str(), level + 1, zoom_range);
			} else {
				if (level == 0) {
					fprintf(stderr, "ERROR: Directory structure in '%s' should be zoom/x/y\n", name);
					exit(EXIT_FAILURE);
				}

				if (level == 1) {
					fprintf(stderr, "ERROR: Directory structure in '%s' should be zoom/x/y\n", split_slash(name)[0].c_str());
					exit(EXIT_FAILURE);
				}

				if (zoom_range == 0) {
					readers->pbf_path.push_back(path);

					if (readers->pbf_path.size() > 1) {
						path_parts1 = split_slash(readers->pbf_path[readers->pbf_path.size() - 1]);
						path_parts2 = split_slash(readers->pbf_path[readers->pbf_path.size() - 2]);
						int p1 = path_parts1.size();
						int p2 = path_parts2.size();

						if (std::stoll(path_parts1[p1 - 3]) == std::stoll(path_parts2[p2 - 3]) && std::stoll(path_parts1[p1 - 2]) == std::stoll(path_parts2[p2 - 2])) {
							readers->z_flag = 0;
							readers->pbf_count++;
						}

						path_parts1.clear();
						path_parts2.clear();
					}
				} else {
					readers->large_zoom.push_back(path);

					if (readers->large_zoom.size() > 1) {
						path_parts1 = split_slash(readers->large_zoom[readers->large_zoom.size() - 1]);
						path_parts2 = split_slash(readers->large_zoom[readers->large_zoom.size() - 2]);
						int p1 = path_parts1.size();
						int p2 = path_parts2.size();

						if (std::stoll(path_parts1[p1 - 3]) == std::stoll(path_parts2[p2 - 3]) && std::stoll(path_parts1[p1 - 2]) == std::stoll(path_parts2[p2 - 2])) {
							readers->z_flag = 1;
							readers->pbf_count++;
						}

						path_parts1.clear();
						path_parts2.clear();
					}
				}
			}

			free(namelist[i]);
			i++;
		}

		if (level == 0) {
			if (readers->pbf_count > 0) {
				std::sort(readers->pbf_path.end() - (readers->pbf_count + 1), readers->pbf_path.end(), std::greater<std::string>());
			}

			readers->pbf_path.insert(std::end(readers->pbf_path), std::begin(readers->large_zoom), std::end(readers->large_zoom));
		}

		free(namelist);
	} else if (n == 0) {
		fprintf(stderr, "ERROR: Empty directory '%s'\n", name);
		exit(EXIT_FAILURE);
	} else {
		perror("scandir");
	}

	return readers;
}

struct reader *begin_reading(char *fname) {
	DIR *dir;
	struct reader *r = new reader;
	if ((dir = opendir(fname)) != NULL) {
		r = read_dir(r, fname, 0, 0);

		std::vector<std::string> path_parts;
		path_parts = split_slash(r->pbf_path[0]);
		int p = path_parts.size();

		r->db = NULL;
		r->stmt = NULL;
		r->next = NULL;
		r->pbf_count = 0;
		r->zoom = std::stoll(path_parts[p - 3]);
		r->x = std::stoll(path_parts[p - 2]);
		r->y = std::stoll(path_parts[p - 1].substr(0, path_parts[p - 1].find_last_of(".")));
		r->sorty = (1LL << r->zoom) - 1 - r->y;
		r->data = dir_read_tile(r->pbf_path[0]);
		path_parts.clear();
		closedir(dir);
	} else {
		sqlite3 *db;

		if (sqlite3_open(fname, &db) != SQLITE_OK) {
			fprintf(stderr, "%s: %s\n", fname, sqlite3_errmsg(db));
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

struct zxy {
	long long z;
	long long x;
	long long y;

	zxy(long long _z, long long _x, long long _y) {
		z = _z;
		x = _x;
		y = _y;
	}

	bool operator<(zxy const &other) const {
		if (z < other.z) {
			return true;
		}
		if (z > other.z) {
			return false;
		}

		if (x < other.x) {
			return true;
		}
		if (x > other.x) {
			return false;
		}

		if (y < other.y) {
			return true;
		}

		return false;
	}
};

struct arg {
	std::map<zxy, std::vector<std::string>> inputs;
	std::map<zxy, std::string> outputs;

	std::map<std::string, layermap_entry> *layermap;

	std::vector<std::string> *header;
	std::map<std::string, std::vector<std::string>> *mapping;
	std::set<std::string> *exclude;
	std::set<std::string> *keep_layers;
	std::set<std::string> *remove_layers;
	int ifmatched;
};

void *join_worker(void *v) {
	arg *a = (arg *) v;

	for (auto ai = a->inputs.begin(); ai != a->inputs.end(); ++ai) {
		mvt_tile tile;

		for (size_t i = 0; i < ai->second.size(); i++) {
			handle(ai->second[i], ai->first.z, ai->first.x, ai->first.y, *(a->layermap), *(a->header), *(a->mapping), *(a->exclude), *(a->keep_layers), *(a->remove_layers), a->ifmatched, tile);
		}

		ai->second.clear();

		bool anything = false;
		for (size_t i = 0; i < tile.layers.size(); i++) {
			if (tile.layers[i].features.size() > 0) {
				anything = true;
				break;
			}
		}

		if (anything) {
			std::string pbf = tile.encode();
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

void handle_tasks(std::map<zxy, std::vector<std::string>> &tasks, std::vector<std::map<std::string, layermap_entry>> &layermaps, sqlite3 *outdb, const char *outdir, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping, std::set<std::string> &exclude, int ifmatched, std::set<std::string> &keep_layers, std::set<std::string> &remove_layers) {
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
	}

	size_t count = 0;
	// This isn't careful about distributing tasks evenly across CPUs,
	// but, from testing, it actually takes a little longer to do
	// the proper allocation than is saved by perfectly balanced threads.
	for (auto ai = tasks.begin(); ai != tasks.end(); ++ai) {
		args[count].inputs.insert(*ai);
		count = (count + 1) % CPUS;

		if (ai == tasks.begin()) {
			fprintf(stderr, "%lld/%lld/%lld  \r", ai->first.z, ai->first.x, ai->first.y);
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

void decode(struct reader *readers, char *map, std::map<std::string, layermap_entry> &layermap, sqlite3 *outdb, const char *outdir, struct stats *st, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping, std::set<std::string> &exclude, int ifmatched, std::string &attribution, std::string &description, std::set<std::string> &keep_layers, std::set<std::string> &remove_layers, std::string &name) {
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

		zxy tile = zxy(r->zoom, r->x, r->y);
		if (tasks.count(tile) == 0) {
			tasks.insert(std::pair<zxy, std::vector<std::string>>(tile, std::vector<std::string>()));
		}
		auto f = tasks.find(tile);
		f->second.push_back(r->data);

		if (readers == NULL || readers->zoom != r->zoom || readers->x != r->x || readers->y != r->y) {
			if (tasks.size() > 100 * CPUS) {
				handle_tasks(tasks, layermaps, outdb, outdir, header, mapping, exclude, ifmatched, keep_layers, remove_layers);
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
			r->pbf_count++;

			if (r->pbf_count != static_cast<int>(r->pbf_path.size())) {
				std::vector<std::string> path_parts;
				path_parts = split_slash(r->pbf_path[r->pbf_count]);
				int p = path_parts.size();
				r->zoom = std::stoll(path_parts[p - 3]);
				r->x = std::stoll(path_parts[p - 2]);
				r->y = std::stoll(path_parts[p - 1].substr(0, path_parts[p - 1].find_last_of(".")));
				r->sorty = (1LL << r->zoom) - 1 - r->y;
				r->data = dir_read_tile(r->pbf_path[r->pbf_count]);
				path_parts.clear();
			} else {
				r->zoom = 32;
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

	handle_tasks(tasks, layermaps, outdb, outdir, header, mapping, exclude, ifmatched, keep_layers, remove_layers);
	layermap = merge_layermaps(layermaps);

	struct reader *next;
	for (struct reader *r = readers; r != NULL; r = next) {
		next = r->next;

		if (r->db != NULL) {
			sqlite3_finalize(r->stmt);

			if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'minzoom'", -1, &r->stmt, NULL) == SQLITE_OK) {
				if (sqlite3_step(r->stmt) == SQLITE_ROW) {
					int minzoom = sqlite3_column_int(r->stmt, 0);
					st->minzoom = min(st->minzoom, minzoom);
				}
				sqlite3_finalize(r->stmt);
			}
			if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'maxzoom'", -1, &r->stmt, NULL) == SQLITE_OK) {
				if (sqlite3_step(r->stmt) == SQLITE_ROW) {
					int maxzoom = sqlite3_column_int(r->stmt, 0);
					st->maxzoom = max(st->maxzoom, maxzoom);
				}
				sqlite3_finalize(r->stmt);
			}
			if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'center'", -1, &r->stmt, NULL) == SQLITE_OK) {
				if (sqlite3_step(r->stmt) == SQLITE_ROW) {
					const unsigned char *s = sqlite3_column_text(r->stmt, 0);
					sscanf((char *) s, "%lf,%lf", &st->midlon, &st->midlat);
				}
				sqlite3_finalize(r->stmt);
			}
			if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'attribution'", -1, &r->stmt, NULL) == SQLITE_OK) {
				if (sqlite3_step(r->stmt) == SQLITE_ROW) {
					attribution = std::string((char *) sqlite3_column_text(r->stmt, 0));
				}
				sqlite3_finalize(r->stmt);
			}
			if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'description'", -1, &r->stmt, NULL) == SQLITE_OK) {
				if (sqlite3_step(r->stmt) == SQLITE_ROW) {
					description = std::string((char *) sqlite3_column_text(r->stmt, 0));
				}
				sqlite3_finalize(r->stmt);
			}
			if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'name'", -1, &r->stmt, NULL) == SQLITE_OK) {
				if (sqlite3_step(r->stmt) == SQLITE_ROW) {
					if (name.size() == 0) {
						name = std::string((char *) sqlite3_column_text(r->stmt, 0));
					} else {
						name += " + " + std::string((char *) sqlite3_column_text(r->stmt, 0));
					}
				}
				sqlite3_finalize(r->stmt);
			}
			if (sqlite3_prepare_v2(r->db, "SELECT value from metadata where name = 'bounds'", -1, &r->stmt, NULL) == SQLITE_OK) {
				if (sqlite3_step(r->stmt) == SQLITE_ROW) {
					const unsigned char *s = sqlite3_column_text(r->stmt, 0);
					if (sscanf((char *) s, "%lf,%lf,%lf,%lf", &minlon, &minlat, &maxlon, &maxlat) == 4) {
						st->minlon = min(minlon, st->minlon);
						st->maxlon = max(maxlon, st->maxlon);
						st->minlat = min(minlat, st->minlat);
						st->maxlat = max(maxlat, st->maxlat);
					}
				}
				sqlite3_finalize(r->stmt);
			}

			if (sqlite3_close(r->db) != SQLITE_OK) {
				fprintf(stderr, "Could not close database: %s\n", sqlite3_errmsg(r->db));
				exit(EXIT_FAILURE);
			}

		} else {
			std::vector<std::string> path_parts;
			path_parts = split_slash(r->pbf_path[0]);
			std::string metadata_path = path_parts[0];

			for (int i = 1; i < static_cast<int>(path_parts.size()) - 3; i++) {
				metadata_path = metadata_path + "/" + path_parts[i];
			}

			metadata_path += "/metadata.json";

			path_parts.clear();
			FILE *f = fopen(metadata_path.c_str(), "r");

			if (f == NULL) {
				perror(metadata_path.c_str());
				exit(EXIT_FAILURE);
			}

			json_pull *jp = json_begin_file(f);
			json_object *j, *k;

			while ((j = json_read(jp)) != NULL) {
				if (j->type == JSON_HASH) {
					if ((k = json_hash_get(j, "minzoom")) != NULL) {
						const std::string minzoom_tmp = k->string;
						int minzoom = std::stoi(minzoom_tmp);
						st->minzoom = min(st->minzoom, minzoom);
					}

					if ((k = json_hash_get(j, "maxzoom")) != NULL) {
						const std::string maxzoom_tmp = k->string;
						int maxzoom = std::stoi(maxzoom_tmp);
						st->maxzoom = max(st->maxzoom, maxzoom);
					}

					if ((k = json_hash_get(j, "center")) != NULL) {
						const std::string center = k->string;
						const unsigned char *s = (const unsigned char *) center.c_str();
						sscanf((char *) s, "%lf,%lf", &st->midlon, &st->midlat);
					}

					if ((k = json_hash_get(j, "attribution")) != NULL) {
						attribution = k->string;
					}

					if ((k = json_hash_get(j, "description")) != NULL) {
						description = k->string;
					}

					if ((k = json_hash_get(j, "name")) != NULL) {
						const std::string name_tmp = k->string;
						if (name.size() == 0) {
							name = name_tmp;
						} else {
							name += " + " + name_tmp;
						}
					}

					if ((k = json_hash_get(j, "bounds")) != NULL) {
						const std::string bounds = k->string;
						const unsigned char *s = (const unsigned char *) bounds.c_str();
						if (sscanf((char *) s, "%lf,%lf,%lf,%lf", &minlon, &minlat, &maxlon, &maxlat) == 4) {
							st->minlon = min(minlon, st->minlon);
							st->maxlon = max(maxlon, st->maxlon);
							st->minlat = min(minlat, st->minlat);
							st->maxlat = max(maxlat, st->maxlat);
						}
					}
				}
			}
			json_free(j);
			json_end(jp);
			fclose(f);
		}

		delete r;
	}
}

void usage(char **argv) {
	fprintf(stderr, "Usage: %s [-f] [-i] [-pk] [-pC] [-c joins.csv] [-x exclude ...] -o new.mbtiles source.mbtiles ...\n", argv[0]);
	exit(EXIT_FAILURE);
}

#define MAXLINE 10000 /* XXX */

std::vector<std::string> split(char *s) {
	std::vector<std::string> ret;

	while (*s && *s != '\n' && *s != '\r') {
		char *start = s;
		int within = 0;

		for (; *s && *s != '\n' && *s != '\r'; s++) {
			if (*s == '"') {
				within = !within;
			}

			if (*s == ',' && !within) {
				break;
			}
		}

		std::string v = std::string(start, s - start);
		ret.push_back(v);

		if (*s == ',') {
			s++;

			while (*s && isspace(*s)) {
				s++;
			}
		}
	}

	return ret;
}

std::string dequote(std::string s) {
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '"') {
			if (i + 1 < s.size() && s[i + 1] == '"') {
				out.push_back('"');
			}
		} else {
			out.push_back(s[i]);
		}
	}
	return out;
}

void readcsv(char *fn, std::vector<std::string> &header, std::map<std::string, std::vector<std::string>> &mapping) {
	FILE *f = fopen(fn, "r");
	if (f == NULL) {
		perror(fn);
		exit(EXIT_FAILURE);
	}

	char s[MAXLINE];
	if (fgets(s, MAXLINE, f)) {
		header = split(s);

		for (size_t i = 0; i < header.size(); i++) {
			header[i] = dequote(header[i]);
		}
	}
	while (fgets(s, MAXLINE, f)) {
		std::vector<std::string> line = split(s);
		if (line.size() > 0) {
			line[0] = dequote(line[0]);
		}

		for (size_t i = 0; i < line.size() && i < header.size(); i++) {
			// printf("putting %s\n", line[0].c_str());
			mapping.insert(std::pair<std::string, std::vector<std::string>>(line[0], line));
		}
	}

	if (fclose(f) != 0) {
		perror("fclose");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	char *out_mbtiles = NULL;
	char *out_dir = NULL;
	sqlite3 *outdb = NULL;
	char *csv = NULL;
	int force = 0;
	int ifmatched = 0;

	CPUS = sysconf(_SC_NPROCESSORS_ONLN);
	if (CPUS < 1) {
		CPUS = 1;
	}

	std::vector<std::string> header;
	std::map<std::string, std::vector<std::string>> mapping;

	std::set<std::string> exclude;
	std::set<std::string> keep_layers;
	std::set<std::string> remove_layers;

	std::string set_name, set_description, set_attribution;

	extern int optind;
	extern char *optarg;
	int i;

	while ((i = getopt(argc, argv, "fo:e:c:x:ip:l:L:A:N:n:")) != -1) {
		switch (i) {
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

		case 'p':
			if (strcmp(optarg, "k") == 0) {
				pk = true;
			} else if (strcmp(optarg, "C") == 0) {
				pC = true;
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

		case 'l':
			keep_layers.insert(std::string(optarg));
			break;

		case 'L':
			remove_layers.insert(std::string(optarg));
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

	if (out_mbtiles != NULL) {
		if (force) {
			unlink(out_mbtiles);
		}
		outdb = mbtiles_open(out_mbtiles, argv, 0);
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

	decode(readers, csv, layermap, outdb, out_dir, &st, header, mapping, exclude, ifmatched, attribution, description, keep_layers, remove_layers, name);

	if (set_attribution.size() != 0) {
		attribution = set_attribution;
	}
	if (set_description.size() != 0) {
		description = set_description;
	}
	if (set_name.size() != 0) {
		name = set_name;
	}

	mbtiles_write_metadata(outdb, out_dir, name.c_str(), st.minzoom, st.maxzoom, st.minlat, st.minlon, st.maxlat, st.maxlon, st.midlat, st.midlon, 0, attribution.size() != 0 ? attribution.c_str() : NULL, layermap, true, description.c_str());

	if (outdb != NULL) {
		mbtiles_close(outdb, argv);
	}

	return 0;
}
