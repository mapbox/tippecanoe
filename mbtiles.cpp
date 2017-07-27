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
#include "mvt.hpp"
#include "mbtiles.hpp"
#include "text.hpp"

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
		fprintf(stderr, "%s: create metadata table: %s\n", argv[0], err);
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

static void quote(std::string *buf, const char *s) {
	char tmp[strlen(s) * 8 + 1];
	char *out = tmp;

	for (; *s != '\0'; s++) {
		unsigned char ch = (unsigned char) *s;

		if (ch == '\\' || ch == '\"') {
			*out++ = '\\';
			*out++ = ch;
		} else if (ch < ' ') {
			sprintf(out, "\\u%04x", ch);
			out = out + strlen(out);
		} else {
			*out++ = ch;
		}
	}

	*out = '\0';
	buf->append(tmp, strlen(tmp));
}

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

std::string tilestats(std::map<std::string, layermap_entry> const &layermap1) {
	// Consolidate layers/attributes whose names are truncated
	std::vector<std::map<std::string, layermap_entry>> lmv;
	lmv.push_back(layermap1);
	std::map<std::string, layermap_entry> layermap = merge_layermaps(lmv, true);

	std::string out = "{\n";

	out.append("\t\"layerCount\": ");
	out.append(std::to_string(layermap.size()));
	out.append(",\n");

	out.append("\t\"layers\": [\n");

	bool first = true;
	for (auto layer : layermap) {
		if (!first) {
			out.append(",\n");
		}
		first = false;

		out.append("\t\t{\n");

		out.append("\t\t\t\"layer\": \"");
		quote(&out, layer.first.c_str());
		out.append("\",\n");

		out.append("\t\t\t\"count\": ");
		out.append(std::to_string(layer.second.points + layer.second.lines + layer.second.polygons));
		out.append(",\n");

		std::string geomtype = "Polygon";
		if (layer.second.points >= layer.second.lines && layer.second.points >= layer.second.polygons) {
			geomtype = "Point";
		} else if (layer.second.lines >= layer.second.polygons && layer.second.lines >= layer.second.points) {
			geomtype = "LineString";
		}

		out.append("\t\t\t\"geometry\": \"");
		quote(&out, geomtype.c_str());
		out.append("\",\n");

		size_t attrib_count = layer.second.file_keys.size();
		if (attrib_count > 1000) {
			attrib_count = 1000;
		}

		out.append("\t\t\t\"attributeCount\": ");
		out.append(std::to_string(attrib_count));
		out.append(",\n");

		out.append("\t\t\t\"attributes\": [\n");

		size_t attrs = 0;
		for (auto attribute : layer.second.file_keys) {
			if (attrs == 100) {
				break;
			}
			if (attrs != 0) {
				out.append(",\n");
			}
			attrs++;

			out.append("\t\t\t\t{\n");

			out.append("\t\t\t\t\t\"attribute\": \"");
			quote(&out, attribute.first.c_str());
			out.append("\",\n");

			size_t val_count = attribute.second.sample_values.size();
			if (val_count > 1000) {
				val_count = 1000;
			}

			out.append("\t\t\t\t\t\"count\": ");
			out.append(std::to_string(val_count));
			out.append(",\n");

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

			out.append("\t\t\t\t\t\"type\": \"");
			quote(&out, type_str.c_str());
			out.append("\",\n");

			out.append("\t\t\t\t\t\"values\": [\n");

			size_t vals = 0;
			for (auto value : attribute.second.sample_values) {
				if (vals == 100) {
					break;
				}
				if (vals != 0) {
					out.append(",\n");
				}
				vals++;

				if (value.type == mvt_double || value.type == mvt_bool) {
					out.append("\t\t\t\t\t\t");
					out.append(value.string);
				} else {
					std::string trunc = truncate16(value.string, 256);

					if (trunc.size() == value.string.size()) {
						out.append("\t\t\t\t\t\t\"");
						quote(&out, value.string.c_str());
						out.append("\"");
					}
				}
			}

			out.append("\n");
			out.append("\t\t\t\t\t]");

			if ((type & (1 << mvt_double)) != 0) {
				out.append(",\n");

				out.append("\t\t\t\t\t\"min\": ");
				out.append(std::to_string(attribute.second.min));
				out.append(",\n");

				out.append("\t\t\t\t\t\"max\": ");
				out.append(std::to_string(attribute.second.max));
			}

			out.append("\n");
			out.append("\t\t\t\t}");
		}

		out.append("\n\t\t\t]\n");
		out.append("\t\t}");
	}

	out.append("\n");
	out.append("\t]\n");
	out.append("}");

	std::string out2;

	for (size_t i = 0; i < out.size(); i++) {
		if (out[i] != '\t' && out[i] != '\n') {
			out2.push_back(out[i]);
		}
	}

	return out2;
}

void mbtiles_write_metadata(sqlite3 *outdb, const char *outdir, const char *fname, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, layermap_entry> const &layermap, bool vector, const char *description) {
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

	if (vector) {
		std::string buf("{");
		aprintf(&buf, "\"vector_layers\": [ ");

		std::vector<std::string> lnames;
		for (auto ai = layermap.begin(); ai != layermap.end(); ++ai) {
			lnames.push_back(ai->first);
		}

		for (size_t i = 0; i < lnames.size(); i++) {
			if (i != 0) {
				aprintf(&buf, ", ");
			}

			auto fk = layermap.find(lnames[i]);
			aprintf(&buf, "{ \"id\": \"");
			quote(&buf, lnames[i].c_str());
			aprintf(&buf, "\", \"description\": \"\", \"minzoom\": %d, \"maxzoom\": %d, \"fields\": {", fk->second.minzoom, fk->second.maxzoom);

			bool first = true;
			for (auto j = fk->second.file_keys.begin(); j != fk->second.file_keys.end(); ++j) {
				if (first) {
					first = false;
				} else {
					aprintf(&buf, ", ");
				}

				aprintf(&buf, "\"");
				quote(&buf, j->first.c_str());

				int type = 0;
				for (auto s : j->second.sample_values) {
					type |= (1 << s.type);
				}

				if (type == (1 << mvt_double)) {
					aprintf(&buf, "\": \"Number\"");
				} else if (type == (1 << mvt_bool)) {
					aprintf(&buf, "\": \"Boolean\"");
				} else if (type == (1 << mvt_string)) {
					aprintf(&buf, "\": \"String\"");
				} else {
					aprintf(&buf, "\": \"Mixed\"");
				}
			}

			aprintf(&buf, "} }");
		}

		aprintf(&buf, " ],");
		aprintf(&buf, "\"tilestats\": %s", tilestats(layermap).c_str());
		aprintf(&buf, "}");

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
		FILE *fp = fopen(metadata.c_str(), "w");
		if (fp == NULL) {
			perror(metadata.c_str());
			exit(EXIT_FAILURE);
		}

		fprintf(fp, "{\n");

		sqlite3_stmt *stmt;
		bool first = true;
		if (sqlite3_prepare_v2(db, "SELECT name, value from metadata;", -1, &stmt, NULL) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				std::string key, value;

				quote(&key, (const char *) sqlite3_column_text(stmt, 0));
				quote(&value, (const char *) sqlite3_column_text(stmt, 1));

				if (!first) {
					fprintf(fp, ",\n");
				}
				fprintf(fp, "    \"%s\": \"%s\"", key.c_str(), value.c_str());
				first = false;
			}
			sqlite3_finalize(stmt);
		}

		fprintf(fp, "\n}\n");
		fclose(fp);
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
			std::string layername = map->first;
			if (trunc) {
				layername = truncate16(layername, 256);
			}

			if (out.count(layername) == 0) {
				out.insert(std::pair<std::string, layermap_entry>(layername, layermap_entry(out.size())));
				auto out_entry = out.find(layername);
				out_entry->second.minzoom = map->second.minzoom;
				out_entry->second.maxzoom = map->second.maxzoom;
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

							if (fk2->second.sample_values.size() > 1000) {
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

		if (fka->second.sample_values.size() > 1000) {
			fka->second.sample_values.pop_back();
		}
	}

	fka->second.type |= (1 << val.type);
}
