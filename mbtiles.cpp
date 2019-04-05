// for vasprintf() on Linux
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <sys/stat.h>
#include "mvt.hpp"
#include "mbtiles.hpp"
#include "text.hpp"
#include "milo/dtoa_milo.h"
#include "write_json.hpp"
#include "version.hpp"

size_t max_tilestats_attributes = 1000;
size_t max_tilestats_sample_values = 1000;
size_t max_tilestats_values = 100;

sqlite3 *mbtiles_open(char *dbname, char **argv, int forcetable) {
	sqlite3 *outdb;

	if (sqlite3_open(dbname, &outdb) != SQLITE_OK) {
		fprintf(stderr, "%s: %s: %s\n", argv[0], dbname, sqlite3_errmsg(outdb));
		exit(EXIT_FAILURE);
	}

	char *err = NULL;
	if (sqlite3_exec(outdb, "PRAGMA synchronous=0", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: async: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: async: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "PRAGMA journal_mode=DELETE", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: async: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_exec(outdb, "CREATE TABLE metadata (name text, value text);", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: Tileset \"%s\" already exists. You can use --force if you want to delete the old tileset.\n", argv[0], dbname);
		fprintf(stderr, "%s: %s\n", argv[0], err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	if (sqlite3_exec(outdb, "CREATE TABLE tiles (zoom_level integer, tile_column integer, tile_row integer, tile_data blob);", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: create tiles table: %s\n", argv[0], err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	if (sqlite3_exec(outdb, "create unique index name on metadata (name);", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: index metadata: %s\n", argv[0], err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	if (sqlite3_exec(outdb, "create unique index tile_index on tiles (zoom_level, tile_column, tile_row);", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: index tiles: %s\n", argv[0], err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}

	return outdb;
}

void mbtiles_write_tile(sqlite3 *outdb, int z, int tx, int ty, const char *data, int size) {
	sqlite3_stmt *stmt;
	const char *query = "insert into tiles (zoom_level, tile_column, tile_row, tile_data) values (?, ?, ?, ?)";
	if (sqlite3_prepare_v2(outdb, query, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "sqlite3 insert prep failed\n");
		exit(EXIT_FAILURE);
	}

	sqlite3_bind_int(stmt, 1, z);
	sqlite3_bind_int(stmt, 2, tx);
	sqlite3_bind_int(stmt, 3, (1 << z) - 1 - ty);
	sqlite3_bind_blob(stmt, 4, data, size, NULL);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		fprintf(stderr, "sqlite3 insert failed: %s\n", sqlite3_errmsg(outdb));
	}
	if (sqlite3_finalize(stmt) != SQLITE_OK) {
		fprintf(stderr, "sqlite3 finalize failed: %s\n", sqlite3_errmsg(outdb));
	}
}

bool type_and_string::operator<(const type_and_string &o) const {
	if (string < o.string) {
		return true;
	}
	if (string == o.string && type < o.type) {
		return true;
	}
	return false;
}

bool type_and_string::operator!=(const type_and_string &o) const {
	if (type != o.type) {
		return true;
	}
	if (string != o.string) {
		return true;
	}
	return false;
}

void tilestats(std::map<std::string, layermap_entry> const &layermap1, size_t elements, json_writer &state) {
	// Consolidate layers/attributes whose names are truncated
	std::vector<std::map<std::string, layermap_entry>> lmv;
	lmv.push_back(layermap1);
	std::map<std::string, layermap_entry> layermap = merge_layermaps(lmv, true);

	state.json_write_hash();

	state.nospace = true;
	state.json_write_string("layerCount");
	state.json_write_unsigned(layermap.size());

	state.nospace = true;
	state.json_write_string("layers");
	state.json_write_array();

	bool first = true;
	for (auto layer : layermap) {
		first = false;

		state.nospace = true;
		state.json_write_hash();

		state.nospace = true;
		state.json_write_string("layer");
		state.json_write_string(layer.first);

		state.nospace = true;
		state.json_write_string("count");
		state.json_write_unsigned(layer.second.points + layer.second.lines + layer.second.polygons);

		std::string geomtype = "Polygon";
		if (layer.second.points >= layer.second.lines && layer.second.points >= layer.second.polygons) {
			geomtype = "Point";
		} else if (layer.second.lines >= layer.second.polygons && layer.second.lines >= layer.second.points) {
			geomtype = "LineString";
		}

		state.nospace = true;
		state.json_write_string("geometry");
		state.json_write_string(geomtype);

		size_t attrib_count = layer.second.file_keys.size();
		if (attrib_count > max_tilestats_attributes) {
			attrib_count = max_tilestats_attributes;
		}

		state.nospace = true;
		state.json_write_string("attributeCount");
		state.json_write_unsigned(attrib_count);

		state.nospace = true;
		state.json_write_string("attributes");
		state.nospace = true;
		state.json_write_array();

		size_t attrs = 0;
		for (auto attribute : layer.second.file_keys) {
			if (attrs == elements) {
				break;
			}
			attrs++;

			state.nospace = true;
			state.json_write_hash();

			state.nospace = true;
			state.json_write_string("attribute");
			state.json_write_string(attribute.first);

			size_t val_count = attribute.second.sample_values.size();
			if (val_count > max_tilestats_sample_values) {
				val_count = max_tilestats_sample_values;
			}

			state.nospace = true;
			state.json_write_string("count");
			state.json_write_unsigned(val_count);

			int type = 0;
			for (auto s : attribute.second.sample_values) {
				type |= (1 << s.type);
			}

			std::string type_str;
			// No "null" because null attributes are dropped
			if (type == (1 << mvt_double)) {
				type_str = "number";
			} else if (type == (1 << mvt_bool)) {
				type_str = "boolean";
			} else if (type == (1 << mvt_string)) {
				type_str = "string";
			} else {
				type_str = "mixed";
			}

			state.nospace = true;
			state.json_write_string("type");
			state.json_write_string(type_str);

			state.nospace = true;
			state.json_write_string("values");
			state.json_write_array();

			size_t vals = 0;
			for (auto value : attribute.second.sample_values) {
				if (vals == elements) {
					break;
				}

				state.nospace = true;

				if (value.type == mvt_double || value.type == mvt_bool) {
					vals++;

					state.json_write_stringified(value.string);
				} else {
					std::string trunc = truncate16(value.string, 256);

					if (trunc.size() == value.string.size()) {
						vals++;

						state.json_write_string(value.string);
					}
				}
			}

			state.nospace = true;
			state.json_end_array();

			if ((type & (1 << mvt_double)) != 0) {
				state.nospace = true;
				state.json_write_string("min");
				state.json_write_number(attribute.second.min);

				state.nospace = true;
				state.json_write_string("max");
				state.json_write_number(attribute.second.max);
			}

			state.nospace = true;
			state.json_end_hash();
		}

		state.nospace = true;
		state.json_end_array();
		state.nospace = true;
		state.json_end_hash();
	}

	state.nospace = true;
	state.json_end_array();
	state.nospace = true;
	state.json_end_hash();
}

void mbtiles_write_metadata(sqlite3 *outdb, const char *outdir, const char *fname, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, layermap_entry> const &layermap, bool vector, const char *description, bool do_tilestats, std::map<std::string, std::string> const &attribute_descriptions, std::string const &program, std::string const &commandline) {
	char *sql, *err;

	sqlite3 *db = outdb;
	if (outdb == NULL) {
		if (sqlite3_open("", &db) != SQLITE_OK) {
			fprintf(stderr, "Temporary db: %s\n", sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}
		if (sqlite3_exec(db, "CREATE TABLE metadata (name text, value text);", NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "Create metadata table: %s\n", err);
			exit(EXIT_FAILURE);
		}
	}

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('name', %Q);", fname);
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set name in metadata: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('description', %Q);", description != NULL ? description : fname);
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set description in metadata: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('version', %d);", 2);
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set version : %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('minzoom', %d);", minzoom);
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set minzoom: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('maxzoom', %d);", maxzoom);
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set maxzoom: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('center', '%f,%f,%d');", midlon, midlat, maxzoom);
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set center: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('bounds', '%f,%f,%f,%f');", minlon, minlat, maxlon, maxlat);
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set bounds: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('type', %Q);", "overlay");
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set type: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	if (attribution != NULL) {
		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('attribution', %Q);", attribution);
		if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set type: %s\n", err);
			if (!forcetable) {
				exit(EXIT_FAILURE);
			}
		}
		sqlite3_free(sql);
	}

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('format', %Q);", vector ? "pbf" : "png");
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set format: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	std::string version = program + " " + VERSION;
	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('generator', %Q);", version.c_str());
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set version: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('generator_options', %Q);", commandline.c_str());
	if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set commandline: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	if (vector) {
		size_t elements = max_tilestats_values;
		std::string buf;

		{
			json_writer state(&buf);

			state.json_write_hash();
			state.nospace = true;

			state.json_write_string("vector_layers");
			state.json_write_array();

			std::vector<std::string> lnames;
			for (auto ai = layermap.begin(); ai != layermap.end(); ++ai) {
				lnames.push_back(ai->first);
			}

			for (size_t i = 0; i < lnames.size(); i++) {
				auto fk = layermap.find(lnames[i]);
				state.json_write_hash();

				state.json_write_string("id");
				state.json_write_string(lnames[i]);

				state.json_write_string("description");
				state.json_write_string(fk->second.description);

				state.json_write_string("minzoom");
				state.json_write_signed(fk->second.minzoom);

				state.json_write_string("maxzoom");
				state.json_write_signed(fk->second.maxzoom);

				state.json_write_string("fields");
				state.json_write_hash();
				state.nospace = true;

				bool first = true;
				for (auto j = fk->second.file_keys.begin(); j != fk->second.file_keys.end(); ++j) {
					if (first) {
						first = false;
					}

					state.json_write_string(j->first);

					auto f = attribute_descriptions.find(j->first);
					if (f == attribute_descriptions.end()) {
						int type = 0;
						for (auto s : j->second.sample_values) {
							type |= (1 << s.type);
						}

						if (type == (1 << mvt_double)) {
							state.json_write_string("Number");
						} else if (type == (1 << mvt_bool)) {
							state.json_write_string("Boolean");
						} else if (type == (1 << mvt_string)) {
							state.json_write_string("String");
						} else {
							state.json_write_string("Mixed");
						}
					} else {
						state.json_write_string(f->second);
					}
				}

				state.nospace = true;
				state.json_end_hash();
				state.json_end_hash();
			}

			state.json_end_array();

			if (do_tilestats && elements > 0) {
				state.nospace = true;
				state.json_write_string("tilestats");
				tilestats(layermap, elements, state);
			}

			state.nospace = true;
			state.json_end_hash();
		}

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('json', %Q);", buf.c_str());
		if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set json: %s\n", err);
			if (!forcetable) {
				exit(EXIT_FAILURE);
			}
		}
		sqlite3_free(sql);
	}

	if (outdir != NULL) {
		std::string metadata = std::string(outdir) + "/metadata.json";

		struct stat st;
		if (stat(metadata.c_str(), &st) == 0) {
			// Leave existing metadata in place with --allow-existing
		} else {
			FILE *fp = fopen(metadata.c_str(), "w");
			if (fp == NULL) {
				perror(metadata.c_str());
				exit(EXIT_FAILURE);
			}

			json_writer state(fp);

			state.json_write_hash();
			state.json_write_newline();

			sqlite3_stmt *stmt;
			bool first = true;
			if (sqlite3_prepare_v2(db, "SELECT name, value from metadata;", -1, &stmt, NULL) == SQLITE_OK) {
				while (sqlite3_step(stmt) == SQLITE_ROW) {
					std::string key, value;

					const char *k = (const char *) sqlite3_column_text(stmt, 0);
					const char *v = (const char *) sqlite3_column_text(stmt, 1);
					if (k == NULL || v == NULL) {
						fprintf(stderr, "Corrupt mbtiles file: null metadata\n");
						exit(EXIT_FAILURE);
					}

					state.json_comma_newline();
					state.json_write_string(k);
					state.json_write_string(v);
					first = false;
				}
				sqlite3_finalize(stmt);
			}

			state.json_write_newline();
			state.json_end_hash();
			state.json_write_newline();
			fclose(fp);
		}
	}

	if (outdb == NULL) {
		if (sqlite3_close(db) != SQLITE_OK) {
			fprintf(stderr, "Could not close temp database: %s\n", sqlite3_errmsg(db));
			exit(EXIT_FAILURE);
		}
	}
}

void mbtiles_close(sqlite3 *outdb, const char *pgm) {
	char *err;

	if (sqlite3_exec(outdb, "ANALYZE;", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: ANALYZE failed: %s\n", pgm, err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_close(outdb) != SQLITE_OK) {
		fprintf(stderr, "%s: could not close database: %s\n", pgm, sqlite3_errmsg(outdb));
		exit(EXIT_FAILURE);
	}
}

std::map<std::string, layermap_entry> merge_layermaps(std::vector<std::map<std::string, layermap_entry>> const &maps) {
	return merge_layermaps(maps, false);
}

std::map<std::string, layermap_entry> merge_layermaps(std::vector<std::map<std::string, layermap_entry>> const &maps, bool trunc) {
	std::map<std::string, layermap_entry> out;

	for (size_t i = 0; i < maps.size(); i++) {
		for (auto map = maps[i].begin(); map != maps[i].end(); ++map) {
			if (map->second.points + map->second.lines + map->second.polygons + map->second.retain == 0) {
				continue;
			}

			std::string layername = map->first;
			if (trunc) {
				layername = truncate16(layername, 256);
			}

			if (out.count(layername) == 0) {
				out.insert(std::pair<std::string, layermap_entry>(layername, layermap_entry(out.size())));
				auto out_entry = out.find(layername);
				out_entry->second.minzoom = map->second.minzoom;
				out_entry->second.maxzoom = map->second.maxzoom;
				out_entry->second.description = map->second.description;
			}

			auto out_entry = out.find(layername);
			if (out_entry == out.end()) {
				fprintf(stderr, "Internal error merging layers\n");
				exit(EXIT_FAILURE);
			}

			for (auto fk = map->second.file_keys.begin(); fk != map->second.file_keys.end(); ++fk) {
				std::string attribname = fk->first;
				if (trunc) {
					attribname = truncate16(attribname, 256);
				}

				auto fk2 = out_entry->second.file_keys.find(attribname);

				if (fk2 == out_entry->second.file_keys.end()) {
					out_entry->second.file_keys.insert(std::pair<std::string, type_and_string_stats>(attribname, fk->second));
				} else {
					for (auto val : fk->second.sample_values) {
						auto pt = std::lower_bound(fk2->second.sample_values.begin(), fk2->second.sample_values.end(), val);
						if (pt == fk2->second.sample_values.end() || *pt != val) {  // not found
							fk2->second.sample_values.insert(pt, val);

							if (fk2->second.sample_values.size() > max_tilestats_sample_values) {
								fk2->second.sample_values.pop_back();
							}
						}
					}

					fk2->second.type |= fk->second.type;

					if (fk->second.min < fk2->second.min) {
						fk2->second.min = fk->second.min;
					}
					if (fk->second.max > fk2->second.max) {
						fk2->second.max = fk->second.max;
					}
				}
			}

			if (map->second.minzoom < out_entry->second.minzoom) {
				out_entry->second.minzoom = map->second.minzoom;
			}
			if (map->second.maxzoom > out_entry->second.maxzoom) {
				out_entry->second.maxzoom = map->second.maxzoom;
			}

			out_entry->second.points += map->second.points;
			out_entry->second.lines += map->second.lines;
			out_entry->second.polygons += map->second.polygons;
		}
	}

	return out;
}

void add_to_file_keys(std::map<std::string, type_and_string_stats> &file_keys, std::string const &attrib, type_and_string const &val) {
	if (val.type == mvt_null) {
		return;
	}

	auto fka = file_keys.find(attrib);
	if (fka == file_keys.end()) {
		file_keys.insert(std::pair<std::string, type_and_string_stats>(attrib, type_and_string_stats()));
		fka = file_keys.find(attrib);
	}

	if (fka == file_keys.end()) {
		fprintf(stderr, "Can't happen (tilestats)\n");
		exit(EXIT_FAILURE);
	}

	if (val.type == mvt_double) {
		double d = atof(val.string.c_str());

		if (d < fka->second.min) {
			fka->second.min = d;
		}
		if (d > fka->second.max) {
			fka->second.max = d;
		}
	}

	auto pt = std::lower_bound(fka->second.sample_values.begin(), fka->second.sample_values.end(), val);
	if (pt == fka->second.sample_values.end() || *pt != val) {  // not found
		fka->second.sample_values.insert(pt, val);

		if (fka->second.sample_values.size() > max_tilestats_sample_values) {
			fka->second.sample_values.pop_back();
		}
	}

	fka->second.type |= (1 << val.type);
}
