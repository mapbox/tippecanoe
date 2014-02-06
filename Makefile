all: jsoncat

jsoncat: jsoncat.o jsonpull.o
	cc -g -Wall -o $@ $^

%.o: %.c
	cc -g -Wall -c $^
