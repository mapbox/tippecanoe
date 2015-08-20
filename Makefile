PREFIX ?= /usr/local
MANDIR ?= /usr/share/man/man1/

all: tippecanoe enumerate decode tile-join

docs: man/tippecanoe.1

install: tippecanoe
	mkdir -p $(PREFIX)/bin
	cp tippecanoe $(PREFIX)/bin/tippecanoe
	cp man/tippecanoe.1 $(MANDIR)

man/tippecanoe.1: README.md
	md2man-roff README.md > man/tippecanoe.1

vector_tile.pb.cc vector_tile.pb.h: vector_tile.proto
	protoc --cpp_out=. vector_tile.proto

PG=

H = $(shell find . '(' -name '*.h' -o -name '*.hh' ')')
C = $(shell find . '(' -name '*.c' -o -name '*.cc' ')')

INCLUDES = -I/usr/local/include
LIBS = -L/usr/local/lib

tippecanoe: geojson.o jsonpull.o vector_tile.pb.o tile.o clip.o pool.o mbtiles.o geometry.o projection.o memfile.o
	g++ $(PG) $(LIBS) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3

enumerate: enumerate.o
	gcc $(PG) $(LIBS) -O3 -g -Wall -o $@ $^ -lsqlite3

decode: decode.o vector_tile.pb.o projection.o
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
