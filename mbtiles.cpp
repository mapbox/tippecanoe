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

bool tilestats_attributes_entry::operator<(const tilestats_attributes_entry &o) const {
	if (string < o.string) {
		return true;
	}
	if (string == o.string && type < o.type) {
		return true;
	}
	return false;
}

bool tilestats_attributes_entry::operator!=(const tilestats_attributes_entry &o) const {
	if (type != o.type) {
		return true;
	}
	if (string != o.string) {
		return true;
	}
	return false;
}

void tilestats(std::map<std::string, tilestats_entry> const &layermap1, size_t elements, json_writer &jw) {
	// Consolidate layers/attributes whose names are truncated
	std::vector<std::map<std::string, tilestats_entry>> lmv;
	lmv.push_back(layermap1);
	std::map<std::string, tilestats_entry> tilestat = merge_layermaps(lmv, true);

	jw.begin_hash();

	jw.nospace = true;
	jw.write_string("layerCount");
	jw.write_unsigned(tilestat.size());

	jw.nospace = true;
	jw.write_string("layers");
	jw.begin_array();

	bool first = true;
	for (auto layer : tilestat) {
		first = false;

		jw.nospace = true;
		jw.begin_hash();

		jw.nospace = true;
		jw.write_string("layer");
		jw.write_string(layer.first);

		jw.nospace = true;
		jw.write_string("count");
		jw.write_unsigned(layer.second.points + layer.second.lines + layer.second.polygons);

		std::string geomtype = "Polygon";
		if (layer.second.points >= layer.second.lines && layer.second.points >= layer.second.polygons) {
			geomtype = "Point";
		} else if (layer.second.lines >= layer.second.polygons && layer.second.lines >= layer.second.points) {
			geomtype = "LineString";
		}

		jw.nospace = true;
		jw.write_string("geometry");
		jw.write_string(geomtype);

		size_t attrib_count = layer.second.tilestats.size();
		if (attrib_count > max_tilestats_attributes) {
			attrib_count = max_tilestats_attributes;
		}

		jw.nospace = true;
		jw.write_string("attributeCount");
		jw.write_unsigned(attrib_count);

		jw.nospace = true;
		jw.write_string("attributes");
		jw.nospace = true;
		jw.begin_array();

		size_t attrs = 0;
		for (auto attribute : layer.second.tilestats) {
			if (attrs == elements) {
				break;
			}
			attrs++;

			jw.nospace = true;
			jw.begin_hash();

			jw.nospace = true;
			jw.write_string("attribute");
			jw.write_string(attribute.first);

			size_t val_count = attribute.second.sample_values.size();
			if (val_count > max_tilestats_sample_values) {
				val_count = max_tilestats_sample_values;
			}

			jw.nospace = true;
			jw.write_string("count");
			jw.write_unsigned(val_count);

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

			jw.nospace = true;
			jw.write_string("type");
			jw.write_string(type_str);

			jw.nospace = true;
			jw.write_string("values");
			jw.begin_array();

			size_t vals = 0;
			for (auto value : attribute.second.sample_values) {
				if (vals == elements) {
					break;
				}

				jw.nospace = true;

				if (value.type == mvt_double || value.type == mvt_bool) {
					vals++;

					jw.write_stringified(value.string);
				} else {
					std::string trunc = truncate16(value.string, 256);

					if (trunc.size() == value.string.size()) {
						vals++;

						jw.write_string(value.string);
					}
				}
			}

			jw.nospace = true;
			jw.end_array();

			if ((type & (1 << mvt_double)) != 0) {
				jw.nospace = true;
				jw.write_string("min");
				jw.write_number(attribute.second.min);

				jw.nospace = true;
				jw.write_string("max");
				jw.write_number(attribute.second.max);
			}

			jw.nospace = true;
			jw.end_hash();
		}

		jw.nospace = true;
		jw.end_array();
		jw.nospace = true;
		jw.end_hash();
	}

	jw.nospace = true;
	jw.end_array();
	jw.nospace = true;
	jw.end_hash();
}

void mbtiles_write_metadata(sqlite3 *outdb, const char *outdir, const char *fname, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, tilestats_entry> const &tilestat, bool vector, const char *description, bool do_tilestats, std::map<std::string, std::string> const &attribute_descriptions, std::string const &program, std::string const &commandline) {
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
			json_writer jw(&buf);

			jw.begin_hash();
			jw.nospace = true;

			jw.write_string("vector_layers");
			jw.begin_array();

			std::vector<std::string> lnames;
			for (auto ai = tilestat.begin(); ai != tilestat.end(); ++ai) {
				lnames.push_back(ai->first);
			}

			for (size_t i = 0; i < lnames.size(); i++) {
				auto ts = tilestat.find(lnames[i]);
				jw.begin_hash();

				jw.write_string("id");
				jw.write_string(lnames[i]);

				jw.write_string("description");
				jw.write_string(ts->second.description);

				jw.write_string("minzoom");
				jw.write_signed(ts->second.minzoom);

				jw.write_string("maxzoom");
				jw.write_signed(ts->second.maxzoom);

				jw.write_string("fields");
				jw.begin_hash();
				jw.nospace = true;

				bool first = true;
				for (auto j = ts->second.tilestats.begin(); j != ts->second.tilestats.end(); ++j) {
					if (first) {
						first = false;
					}

					jw.write_string(j->first);

					auto f = attribute_descriptions.find(j->first);
					if (f == attribute_descriptions.end()) {
						int type = 0;
						for (auto s : j->second.sample_values) {
							type |= (1 << s.type);
						}

						if (type == (1 << mvt_double)) {
							jw.write_string("Number");
						} else if (type == (1 << mvt_bool)) {
							jw.write_string("Boolean");
						} else if (type == (1 << mvt_string)) {
							jw.write_string("String");
						} else {
							jw.write_string("Mixed");
						}
					} else {
						jw.write_string(f->second);
					}
				}

				jw.nospace = true;
				jw.end_hash();
				jw.end_hash();
			}

			jw.end_array();

			if (do_tilestats && elements > 0) {
				jw.nospace = true;
				jw.write_string("tilestats");
				tilestats(tilestat, elements, jw);
			}

			jw.nospace = true;
			jw.end_hash();
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

			json_writer jw(fp);

			jw.begin_hash();
			jw.write_newline();

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

					jw.json_comma_newline();
					jw.write_string(k);
					jw.write_string(v);
					first = false;
				}
				sqlite3_finalize(stmt);
			}

			jw.write_newline();
			jw.end_hash();
			jw.write_newline();
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

std::map<std::string, tilestats_entry> merge_layermaps(std::vector<std::map<std::string, tilestats_entry>> const &maps) {
	return merge_layermaps(maps, false);
}

std::map<std::string, tilestats_entry> merge_layermaps(std::vector<std::map<std::string, tilestats_entry>> const &maps, bool trunc) {
	std::map<std::string, tilestats_entry> out;

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
				out.insert(std::pair<std::string, tilestats_entry>(layername, tilestats_entry(out.size())));
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

			for (auto ts = map->second.tilestats.begin(); ts != map->second.tilestats.end(); ++ts) {
				std::string attribname = ts->first;
				if (trunc) {
					attribname = truncate16(attribname, 256);
				}

				auto fk2 = out_entry->second.tilestats.find(attribname);

				if (fk2 == out_entry->second.tilestats.end()) {
					out_entry->second.tilestats.insert(std::pair<std::string, tilestats_attributes>(attribname, ts->second));
				} else {
					for (auto val : ts->second.sample_values) {
						auto pt = std::lower_bound(fk2->second.sample_values.begin(), fk2->second.sample_values.end(), val);
						if (pt == fk2->second.sample_values.end() || *pt != val) {  // not found
							fk2->second.sample_values.insert(pt, val);

							if (fk2->second.sample_values.size() > max_tilestats_sample_values) {
								fk2->second.sample_values.pop_back();
							}
						}
					}

					fk2->second.type |= ts->second.type;

					if (ts->second.min < fk2->second.min) {
						fk2->second.min = ts->second.min;
					}
					if (ts->second.max > fk2->second.max) {
						fk2->second.max = ts->second.max;
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

void add_to_tilestats(std::map<std::string, tilestats_attributes> &tilestats, std::string const &attrib, tilestats_attributes_entry const &val) {
	if (val.type == mvt_null) {
		return;
	}

	auto tsa = tilestats.find(attrib);
	if (tsa == tilestats.end()) {
		tilestats.insert(std::pair<std::string, tilestats_attributes>(attrib, tilestats_attributes()));
		tsa = tilestats.find(attrib);
	}

	if (tsa == tilestats.end()) {
		fprintf(stderr, "Can't happen (tilestats)\n");
		exit(EXIT_FAILURE);
	}

	if (val.type == mvt_double) {
		double d = atof(val.string.c_str());

		if (d < tsa->second.min) {
			tsa->second.min = d;
		}
		if (d > tsa->second.max) {
			tsa->second.max = d;
		}
	}

	auto pt = std::lower_bound(tsa->second.sample_values.begin(), tsa->second.sample_values.end(), val);
	if (pt == tsa->second.sample_values.end() || *pt != val) {  // not found
		tsa->second.sample_values.insert(pt, val);

		if (tsa->second.sample_values.size() > max_tilestats_sample_values) {
			tsa->second.sample_values.pop_back();
		}
	}

	tsa->second.type |= (1 << val.type);
}
