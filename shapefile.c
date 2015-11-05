#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "memfile.h"
#include "tile.h"

unsigned read32be(char *s) {
	return ((s[0] & 0xFF) << 24) | ((s[1] & 0xFF) << 16) | ((s[2] & 0xFF) << 8) | ((s[3] & 0xFF) << 0);
}

unsigned read32le(char *s) {
	return ((s[0] & 0xFF) << 0) | ((s[1] & 0xFF) << 8) | ((s[2] & 0xFF) << 16) | ((s[3] & 0xFF) << 24);
}

unsigned read16le(char *s) {
	return ((s[0] & 0xFF) << 0) | ((s[1] & 0xFF) << 8);
}

void parse_shapefile(FILE *fp, const char *reading, long long *seq, long long *metapos, long long *geompos, long long *indexpos, struct pool *exclude, struct pool *include, int exclude_all, FILE *metafile, FILE *geomfile, FILE *indexfile, struct memfile *poolfile, struct memfile *treefile, char *fname, int maxzoom, int layer, double droprate, unsigned *file_bbox) {
	char shpheader[100];
	if (fread(shpheader, 1, 100, fp) != 100) {
		perror("read shapefile header");
		return;
	}

	int magic = read32be(shpheader + 0);
	int flen = 2 * read32be(shpheader + 24) - 100;
	int version = read32le(shpheader + 28);

	if (magic != 9994) {
		fprintf(stderr, "%s: bad shapefile magic number %d\n", reading, magic);
		return;
	}
	if (version != 1000) {
		fprintf(stderr, "%s: bad shapefile version number %d\n", reading, version);
		return;
	}

	char dbfname[strlen(reading) + 1];

	strcpy(dbfname, reading);
	strcpy(dbfname + strlen(dbfname) - 4, ".dbf");
	FILE *dbf = fopen(dbfname, "r");
	if (dbf == NULL) {
		perror(dbfname);
		return;
	}

	char dbfheader[32];
	if (fread(dbfheader, 1, 32, dbf) != 32) {
		perror("read dbf header");
		fclose(dbf);
		return;
	}

	int dbheaderlen = read16le(dbfheader + 8);
	int dbreclen = read16le(dbfheader + 10);

	int cols = dbheaderlen - 32;
	char dbcolumns[cols];
	if (fread(dbcolumns, 1, cols, dbf) != cols) {
		perror("read dbf columns");
		fclose(dbf);
		return;
	}

	int dbflen[cols];
	int dbftype[cols];
	char keys[cols][33];
	int i;
	for (i = 0; i < cols / 32; i++) {
		int start = i * 32;
		int end;
		for (end = start; end < start + 10 && dbcolumns[end] != '\0'; end++) {
			;
		}

		memcpy(keys[i], dbcolumns + start, end - start);
		keys[i][end - start] = '\0';

		dbflen[i] = dbcolumns[32 * i + 16] & 0xFF;
		dbftype[i] = dbcolumns[32 * i + 16] & 0xFF;
	}

	char db[dbreclen];
	char header[8];

	while (flen > 0) {
		if (fread(header, 1, 8, fp) != 8) {
			fprintf(stderr, "%s: shapefile ended early\n", reading);
			exit(EXIT_FAILURE);
		}
		flen -= 8;

		int recno = read32be(header);
		int len = 2 * read32be(header + 4);

		char content[len];
		if (fread(content, 1, len, fp) != len) {
			fprintf(stderr, "%s: shapefile ended early\n", reading);
			exit(EXIT_FAILURE);
		}
		flen -= len;

		int type = read32le(content);
		printf("type %d\n", type);

		if (fread(db, 1, dbreclen, dbf) != dbreclen) {
			fprintf(stderr, "%s: metadata ended early\n", dbfname);
			exit(EXIT_FAILURE);
		}
	}

	fclose(dbf);
}
