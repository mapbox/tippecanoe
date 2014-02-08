all: jsoncat

jsoncat: jsoncat.o jsonpull.o
	cc -g -Wall -o $@ $^

jsoncat.o jsonpull.o: jsonpull.h

%.o: %.c
	cc -g -Wall -c $<
