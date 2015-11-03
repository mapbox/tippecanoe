PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man/man1/

all: tippecanoe tippecanoe-enumerate tippecanoe-decode tile-join

docs: man/tippecanoe.1

install: tippecanoe
	mkdir -p $(PREFIX)/bin
	mkdir -p $(MANDIR)
	cp tippecanoe $(PREFIX)/bin/tippecanoe
	cp tippecanoe-enumerate $(PREFIX)/bin/tippecanoe-enumerate
	cp tippecanoe-decode $(PREFIX)/bin/tippecanoe-decode
	cp tile-join $(PREFIX)/bin/tile-join
	cp man/tippecanoe.1 $(MANDIR)/tippecanoe.1

man/tippecanoe.1: README.md
	md2man-roff README.md > man/tippecanoe.1

vector_tile.pb.cc vector_tile.pb.h: vector_tile.proto
	protoc --cpp_out=. vector_tile.proto

PG=

H = $(shell find . '(' -name '*.h' -o -name '*.hh' ')')
C = $(shell find . '(' -name '*.c' -o -name '*.cc' ')')

INCLUDES = -I/usr/local/include
LIBS = -L/usr/local/lib

tippecanoe: geojson.o jsonpull.o vector_tile.pb.o tile.o clip.o pool.o mbtiles.o geometry.o projection.o memfile.o clipper/clipper.o
	g++ $(PG) $(LIBS) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3 -lpthread

tippecanoe-enumerate: enumerate.o
	gcc $(PG) $(LIBS) -O3 -g -Wall -o $@ $^ -lsqlite3

tippecanoe-decode: decode.o vector_tile.pb.o projection.o
	g++ $(PG) $(LIBS) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3

tile-join: tile-join.o vector_tile.pb.o projection.o pool.o mbtiles.o
	g++ $(PG) $(LIBS) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3

libjsonpull.a: jsonpull.o
	ar rc $@ $^
	ranlib $@

%.o: %.c $(H)
	cc $(PG) $(INCLUDES) -O3 -g -Wall -c $<

%.o: %.cc $(H)
	g++ $(PG) $(INCLUDES) -O3 -g -Wall -c $<

clean:
	rm tippecanoe *.o

indent:
	clang-format -i -style="{BasedOnStyle: Google, IndentWidth: 8, UseTab: Always, AllowShortIfStatementsOnASingleLine: false, ColumnLimit: 0, ContinuationIndentWidth: 8, SpaceAfterCStyleCast: true, IndentCaseLabels: false, AllowShortBlocksOnASingleLine: false, AllowShortFunctionsOnASingleLine: false}" $(C) $(H)
