PREFIX=/usr/local

all: tippecanoe

install: tippecanoe
	cp tippecanoe $(PREFIX)/bin/tippecanoe

vector_tile.pb.cc vector_tile.pb.h: vector_tile.proto
	protoc --cpp_out=. vector_tile.proto

PG=

tippecanoe: geojson.o jsonpull.o vector_tile.pb.o tile.o
	cc $(PG) -O3 -g -Wall -o $@ $^ -lm -lz -lprotobuf-lite -lsqlite3

libjsonpull.a: jsonpull.o
	ar rc $@ $^
	ranlib $@

%.o: %.c
	cc $(PG) -O3 -g -Wall -c $<

%.o: %.cc
	g++ $(PG) -O3 -g -Wall -c $<

clean:
	rm tippecanoe *.o
