PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man/man1/

# inherit from env if set
CC := $(CC)
CXX := $(CXX)
CFLAGS := $(CFLAGS)
CXXFLAGS := $(CXXFLAGS)
LDFLAGS := $(LDFLAGS)

all: tippecanoe tippecanoe-enumerate tippecanoe-decode tile-join

docs: man/tippecanoe.1

install: tippecanoe tippecanoe-enumerate tippecanoe-decode tile-join
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
	$(CXX) $(PG) $(LIBS) -O3 -g -Wall $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lprotobuf-lite -lsqlite3 -lpthread

tippecanoe-enumerate: enumerate.o
	$(CC) $(PG) $(LIBS) -O3 -g -Wall $(CFLAGS) -o $@ $^ $(LDFLAGS) -lsqlite3

tippecanoe-decode: decode.o vector_tile.pb.o projection.o
	$(CXX) $(PG) $(LIBS) -O3 -g -Wall $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lprotobuf-lite -lsqlite3

tile-join: tile-join.o vector_tile.pb.o projection.o pool.o mbtiles.o
	$(CXX) $(PG) $(LIBS) -O3 -g -Wall $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lprotobuf-lite -lsqlite3

libjsonpull.a: jsonpull.o
	ar rc $@ $^
	ranlib $@

%.o: %.c $(H)
	cc $(PG) $(INCLUDES) -O3 -g -Wall $(CFLAGS) -c $<

%.o: %.cc $(H)
	$(CXX) $(PG) $(INCLUDES) -O3 -g -Wall $(CXXFLAGS) -c $<

clean:
	rm -f tippecanoe *.o

indent:
	clang-format -i -style="{BasedOnStyle: Google, IndentWidth: 8, UseTab: Always, AllowShortIfStatementsOnASingleLine: false, ColumnLimit: 0, ContinuationIndentWidth: 8, SpaceAfterCStyleCast: true, IndentCaseLabels: false, AllowShortBlocksOnASingleLine: false, AllowShortFunctionsOnASingleLine: false}" $(C) $(H)
