PREFIX=/usr/local

all: jsoncat libjsonpull.a geojson

install: jsonpull.h libjsonpull.a
	cp jsonpull.h $(PREFIX)/include/jsonpull.h
	cp libjsonpull.a $(PREFIX)/lib/libjsonpull.a

vector_tile.pb.cc vector_tile.pb.h: vector_tile.proto
	protoc --cpp_out=. vector_tile.proto

PG=

jsoncat: jsoncat.o jsonpull.o
	cc $(PG) -g -Wall -o $@ $^

geojson: geojson.o jsonpull.o vector_tile.pb.o tile.o
	cc $(PG) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3

jsoncat.o jsonpull.o: jsonpull.h

libjsonpull.a: jsonpull.o
	ar rc $@ $^
	ranlib $@

%.o: %.c
	cc $(PG) -O3 -g -Wall -c $<

%.o: %.cc
	g++ $(PG) -O3 -g -Wall -c $<
