PREFIX=/usr/local

all: jsoncat libjsonpull.a geojson

install: jsonpull.h libjsonpull.a
	cp jsonpull.h $(PREFIX)/include/jsonpull.h
	cp libjsonpull.a $(PREFIX)/lib/libjsonpull.a

jsoncat: jsoncat.o jsonpull.o
	cc -g -Wall -o $@ $^

geojson: geojson.o jsonpull.o
	cc -g -Wall -o $@ $^

jsoncat.o jsonpull.o: jsonpull.h

libjsonpull.a: jsonpull.o
	ar rc $@ $^
	ranlib $@

%.o: %.c
	cc -g -Wall -c $<
