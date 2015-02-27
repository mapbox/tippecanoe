PREFIX ?= /usr/local

all: tippecanoe enumerate decode simplify

install: tippecanoe
	mkdir -p $(PREFIX)/bin
	cp tippecanoe $(PREFIX)/bin/tippecanoe

vector_tile.pb.cc vector_tile.pb.h: vector_tile.proto
	protoc --cpp_out=. vector_tile.proto

PG=

H = $(shell find . '(' -name '*.h' -o -name '*.hh' ')')

INCLUDES = -I/usr/local/include
LIBS = -L/usr/local/lib

tippecanoe: geojson.o jsonpull.o vector_tile.pb.o tile.o clip.o pool.o mbtiles.o geometry.o projection.o
	g++ $(PG) $(LIBS) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3

enumerate: enumerate.o
	gcc $(PG) $(LIBS) -O3 -g -Wall -o $@ $^ -lsqlite3

decode: decode.o vector_tile.pb.o projection.o
	g++ $(PG) $(LIBS) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3

simplify: simplify.o projection.o
	gcc $(PG) $(LIBS) -O3 -g -Wall -o $@ $^

libjsonpull.a: jsonpull.o
	ar rc $@ $^
	ranlib $@

%.o: %.c $(H)
	cc $(PG) $(INCLUDES) -O3 -g -Wall -c $<

%.o: %.cc $(H)
	g++ $(PG) $(INCLUDES) -O3 -g -Wall -c $<

clean:
	rm tippecanoe *.o
