PREFIX=/usr/local

all: jsoncat libjsonpull.a geojson

install: jsonpull.h libjsonpull.a
	cp jsonpull.h $(PREFIX)/include/jsonpull.h
	cp libjsonpull.a $(PREFIX)/lib/libjsonpull.a

PG=

jsoncat: jsoncat.o jsonpull.o
	cc $(PG) -g -Wall -o $@ $^

geojson: geojson.o jsonpull.o
	cc $(PG) -O3 -g -Wall -o $@ $^ -lm

jsoncat.o jsonpull.o: jsonpull.h

libjsonpull.a: jsonpull.o
	ar rc $@ $^
	ranlib $@

%.o: %.c
	cc $(PG) -O3 -g -Wall -c $<
