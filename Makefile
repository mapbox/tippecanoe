PREFIX=/usr/local

all: tippecanoe enumerate decode

install: tippecanoe
	cp tippecanoe $(PREFIX)/bin/tippecanoe

vector_tile.pb.cc vector_tile.pb.h: vector_tile.proto
	protoc --cpp_out=. vector_tile.proto

PG=

H = $(shell find . '(' -name '*.h' -o -name '*.hh' ')')

tippecanoe: geojson.o jsonpull.o vector_tile.pb.o tile.o clip.o pool.o mbtiles.o geometry.o projection.o
	g++ $(PG) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3

enumerate: enumerate.o
	gcc $(PG) -O3 -g -Wall -o $@ $^ -lsqlite3

decode: decode.o vector_tile.pb.o projection.o
	g++ $(PG) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3

libjsonpull.a: jsonpull.o
	ar rc $@ $^
	ranlib $@

%.o: %.c $(H)
	cc $(PG) -O3 -g -Wall -c $<

%.o: %.cc $(H)
	g++ $(PG) -O3 -g -Wall -c $<

clean:
	rm tippecanoe *.o
