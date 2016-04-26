PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man/man1/

# inherit from env if set
CC := $(CC)
CXX := $(CXX)
CFLAGS := $(CFLAGS)
CXXFLAGS := $(CXXFLAGS) -std=c++11
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

PG=

H = $(shell find . '(' -name '*.h' -o -name '*.hh' ')')
C = $(shell find . '(' -name '*.c' -o -name '*.cc' ')')

INCLUDES = -I/usr/local/include -I.
LIBS = -L/usr/local/lib

tippecanoe: geojson.o jsonpull.o tile.o clip.o pool.o mbtiles.o geometry.o projection.o memfile.o clipper/clipper.o mvt.o
	$(CXX) $(PG) $(LIBS) -O3 -g -Wall $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

tippecanoe-enumerate: enumerate.o
	$(CC) $(PG) $(LIBS) -O3 -g -Wall $(CFLAGS) -o $@ $^ $(LDFLAGS) -lsqlite3

tippecanoe-decode: decode.o projection.o mvt.o
	$(CXX) $(PG) $(LIBS) -O3 -g -Wall $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3

tile-join: tile-join.o projection.o pool.o mbtiles.o mvt.o
	$(CXX) $(PG) $(LIBS) -O3 -g -Wall $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3

libjsonpull.a: jsonpull.o
	$(AR) rc $@ $^
	ranlib $@

%.o: %.c $(H)
	$(CC) $(PG) $(INCLUDES) -O3 -g -Wall $(CFLAGS) -c $<

%.o: %.cc $(H)
	$(CXX) $(PG) $(INCLUDES) -O3 -g -Wall $(CXXFLAGS) -c $<

clean:
	rm -f tippecanoe *.o

indent:
	clang-format -i -style="{BasedOnStyle: Google, IndentWidth: 8, UseTab: Always, AllowShortIfStatementsOnASingleLine: false, ColumnLimit: 0, ContinuationIndentWidth: 8, SpaceAfterCStyleCast: true, IndentCaseLabels: false, AllowShortBlocksOnASingleLine: false, AllowShortFunctionsOnASingleLine: false}" $(C) $(H)

geometry.o: clipper/clipper.hpp

TESTS = $(wildcard tests/*/out/*.json)
SPACE = $(NULL) $(NULL)

test: tippecanoe tippecanoe-decode $(addsuffix .check,$(TESTS)) parallel-test pbf-test

# Work around Makefile and filename punctuation limits: _ for space, @ for :, % for /
%.json.check:
	./tippecanoe -ad -f -o $@.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json.check,%,$(word 4,$(subst /, ,$@)))))) $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json) < /dev/null
	./tippecanoe-decode $@.mbtiles > $@.out
	cmp $(patsubst %.check,%,$@) $@.out
	rm $@.out $@.mbtiles

parallel-test:
	mkdir -p tests/parallel
	perl -e 'for ($$i = 0; $$i < 20; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; print "{ \"type\": \"Feature\", \"properties\": { }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in1.json
	perl -e 'for ($$i = 0; $$i < 300000; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; print "{ \"type\": \"Feature\", \"properties\": { }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in2.json
	perl -e 'for ($$i = 0; $$i < 20; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; print "{ \"type\": \"Feature\", \"properties\": { }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in3.json
	./tippecanoe -z5 -f -pi -l test -n test -o tests/parallel/linear-file.mbtiles tests/parallel/in[123].json
	./tippecanoe -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-file.mbtiles tests/parallel/in[123].json
	cat tests/parallel/in[123].json | ./tippecanoe -z5 -f -pi -l test -n test -o tests/parallel/linear-pipe.mbtiles
	cat tests/parallel/in[123].json | ./tippecanoe -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-pipe.mbtiles
	./tippecanoe-decode tests/parallel/linear-file.mbtiles > tests/parallel/linear-file.json
	./tippecanoe-decode tests/parallel/parallel-file.mbtiles > tests/parallel/parallel-file.json
	./tippecanoe-decode tests/parallel/linear-pipe.mbtiles > tests/parallel/linear-pipe.json
	./tippecanoe-decode tests/parallel/parallel-pipe.mbtiles > tests/parallel/parallel-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-file.json
	cmp tests/parallel/linear-file.json tests/parallel/linear-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-pipe.json
	rm tests/parallel/*.mbtiles tests/parallel/*.json

pbf-test:
	./tippecanoe-decode tests/pbf/11-328-791.vector.pbf 11 328 791 > tests/pbf/11-328-791.vector.pbf.out
	cmp tests/pbf/11-328-791.json tests/pbf/11-328-791.vector.pbf.out
	rm tests/pbf/11-328-791.vector.pbf.out

# Use this target to regenerate the standards that the tests are compared against
# after making a change that legitimately changes their output

prep-test: $(TESTS)

tests/%.json: Makefile tippecanoe tippecanoe-decode
	./tippecanoe -f -o $@.check.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json,%,$(word 4,$(subst /, ,$@)))))) $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json)
	./tippecanoe-decode $@.check.mbtiles > $@
	cmp $(patsubst %.check,%,$@) $@
	rm $@.check.mbtiles
