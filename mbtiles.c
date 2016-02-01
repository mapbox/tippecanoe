// for vasprintf() on Linux
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "pool.h"
#include "tile.h"
#include "mbtiles.h"

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

static void quote(char **buf, const char *s) {
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
	*buf = realloc(*buf, strlen(*buf) + strlen(tmp) + 1);
	if (*buf == NULL) {
		perror("realloc");
		exit(EXIT_FAILURE);
	}
	strcat(*buf, tmp);
}

static void aprintf(char **buf, const char *format, ...) {
	va_list ap;
	char *tmp;

	va_start(ap, format);
	if (vasprintf(&tmp, format, ap) < 0) {
		fprintf(stderr, "memory allocation failure\n");
		exit(EXIT_FAILURE);
	}
	va_end(ap);

	*buf = realloc(*buf, strlen(*buf) + strlen(tmp) + 1);
	strcat(*buf, tmp);
	free(tmp);
}

static int pvcmp(const void *v1, const void *v2) {
	const struct pool_val *const *pv1 = v1;
	const struct pool_val *const *pv2 = v2;

	int n = strcmp((*pv1)->s, (*pv2)->s);
	if (n != 0) {
		return n;
	}

	return (*pv1)->type - (*pv2)->type;
}

void mbtiles_write_metadata(sqlite3 *outdb, const char *fname, char **layername, int minzoom, int maxzoom, double minlat, double minlon, double maxlat, double maxlon, double midlat, double midlon, struct pool **file_keys, int nlayers, int forcetable) {
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

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('version', %d);", 1);
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

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('format', %Q);", "pbf");
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set format: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);

	char *buf = strdup("{");
	aprintf(&buf, "\"vector_layers\": [ ");

	int i;
	for (i = 0; i < nlayers; i++) {
		if (i != 0) {
			aprintf(&buf, ", ");
		}

		aprintf(&buf, "{ \"id\": \"");
		quote(&buf, layername[i]);
		aprintf(&buf, "\", \"description\": \"\", \"minzoom\": %d, \"maxzoom\": %d, \"fields\": {", minzoom, maxzoom);

		int n = 0;
		struct pool_val *pv;
		for (pv = file_keys[i]->head; pv != NULL; pv = pv->next) {
			n++;
		}

		struct pool_val *vals[n];
		n = 0;
		for (pv = file_keys[i]->head; pv != NULL; pv = pv->next) {
			vals[n++] = pv;
		}

		qsort(vals, n, sizeof(struct pool_val *), pvcmp);

		int j;
		for (j = 0; j < n; j++) {
			pv = vals[j];
			aprintf(&buf, "\"");
			quote(&buf, pv->s);

			if (pv->type == VT_NUMBER) {
				aprintf(&buf, "\": \"Number\"");
			} else if (pv->type == VT_BOOLEAN) {
				aprintf(&buf, "\": \"Boolean\"");
			} else {
				aprintf(&buf, "\": \"String\"");
			}

			if (j + 1 < n) {
				aprintf(&buf, ", ");
			}
		}

		aprintf(&buf, "} }");
	}

	aprintf(&buf, " ] }");

	sql = sqlite3_mprintf("INSERT INTO metadata (name, value) VALUES ('json', %Q);", buf);
	if (sqlite3_exec(outdb, sql, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "set json: %s\n", err);
		if (!forcetable) {
			exit(EXIT_FAILURE);
		}
	}
	sqlite3_free(sql);
	free(buf);
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
