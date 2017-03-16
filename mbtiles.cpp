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

void mbtiles_write_metadata(sqlite3 *outdb, const char *fname, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, int forcetable, const char *attribution, std::map<std::string, layermap_entry> const &layermap, bool vector) {
	char *sql, *err;

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('name', %Q);", fname);
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set name in metadata: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('description', %Q);", fname);
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set description in metadata: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('version', %d);", 2);
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set version : %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('minzoom', %d);", minzoom);
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set minzoom: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('maxzoom', %d);", maxzoom);
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set maxzoom: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('center', '%f,%f,%d');", midlon, midlat, maxzoom);
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set center: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('bounds', '%f,%f,%f,%f');", minlon, minlat, maxlon, maxlat);
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set bounds: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('type', %Q);", "overlay");
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set type: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	if (attribution != NULL) {
		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('attribution', %Q);", attribution);
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set type: %s\n", err);
			if (!forcetable) {
				exit(EXIT_FAILURE);
			}
		}
		sqlite3_free(sql);
	}

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('format', %Q);", vector ? "pbf" : "png");
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
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

			std::set<type_and_string>::iterator j;
			bool first = true;
			for (j = fk->second.file_keys.begin(); j != fk->second.file_keys.end(); ++j) {
				if (first) {
					first = false;
				} else {
					aprintf(&buf, ", ");
				}

				aprintf(&buf, "\"");
				quote(&buf, j->string.c_str());

				if (j->type == mvt_double ||
				    j->type == mvt_float ||
				    j->type == mvt_double ||
				    j->type == mvt_uint ||
				    j->type == mvt_sint) {
					aprintf(&buf, "\": \"Number\"");
				} else if (j->type == mvt_bool) {
					aprintf(&buf, "\": \"Boolean\"");
				} else {
					aprintf(&buf, "\": \"String\"");
				}
			}

			aprintf(&buf, "} }");
		}

		aprintf(&buf, " ] }");

		sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('json', %Q);", buf.c_str());
		if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "set json: %s\n", err);
			if (!forcetable) {
				exit(EXIT_FAILURE);
			}
		}
		sqlite3_free(sql);
	}
}

void mbtiles_close(sqlite3 *outdb, char **argv) {
	char *err;

	if (sqlite3_exec(outdb, "ANALYZE;", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "%s: ANALYZE failed: %s\n", argv[0], err);
		exit(EXIT_FAILURE);
	}
	if (sqlite3_close(outdb) != SQLITE_OK) {
		fprintf(stderr, "%s: could not close database: %s\n", argv[0], sqlite3_errmsg(outdb));
		exit(EXIT_FAILURE);
	}
}

std::map<std::string, layermap_entry> merge_layermaps(std::vector<std::map<std::string, layermap_entry> > const &maps) {
	std::map<std::string, layermap_entry> out;

	for (size_t i = 0; i < maps.size(); i++) {
		for (auto map = maps[i].begin(); map != maps[i].end(); ++map) {
			if (out.count(map->first) == 0) {
				out.insert(std::pair<std::string, layermap_entry>(map->first, layermap_entry(out.size())));
				auto out_entry = out.find(map->first);
				out_entry->second.minzoom = map->second.minzoom;
				out_entry->second.maxzoom = map->second.maxzoom;
			}

			auto out_entry = out.find(map->first);
			if (out_entry == out.end()) {
				fprintf(stderr, "Internal error merging layers\n");
				exit(EXIT_FAILURE);
			}

			for (auto fk = map->second.file_keys.begin(); fk != map->second.file_keys.end(); ++fk) {
				out_entry->second.file_keys.insert(*fk);
			}

			if (map->second.minzoom < out_entry->second.minzoom) {
				out_entry->second.minzoom = map->second.minzoom;
			}
			if (map->second.maxzoom > out_entry->second.maxzoom) {
				out_entry->second.maxzoom = map->second.maxzoom;
			}
		}
	}

	return out;
}
