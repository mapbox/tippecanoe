PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man/man1/
BUILDTYPE ?= Release
SHELL = /bin/bash

# inherit from env if set
CC := $(CC)
CXX := $(CXX)
CFLAGS := $(CFLAGS)
CXXFLAGS := $(CXXFLAGS) -std=c++11
LDFLAGS := $(LDFLAGS)
WARNING_FLAGS := -Wall -Wshadow -Wsign-compare
RELEASE_FLAGS := -O3 -DNDEBUG
DEBUG_FLAGS := -O0 -DDEBUG -fno-inline-functions -fno-omit-frame-pointer

ifeq ($(BUILDTYPE),Release)
	FINAL_FLAGS := -g $(WARNING_FLAGS) $(RELEASE_FLAGS)
else
	FINAL_FLAGS := -g $(WARNING_FLAGS) $(DEBUG_FLAGS)
endif

all: tippecanoe tippecanoe-enumerate tippecanoe-decode tile-join unit

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

H = $(wildcard *.h) $(wildcard *.hpp)
C = $(wildcard *.c) $(wildcard *.cpp)

INCLUDES = -I/usr/local/include -I.
LIBS = -L/usr/local/lib

tippecanoe: geojson.o jsonpull/jsonpull.o tile.o pool.o mbtiles.o geometry.o projection.o memfile.o clipper/clipper.o mvt.o serial.o main.o text.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

tippecanoe-enumerate: enumerate.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lsqlite3

tippecanoe-decode: decode.o projection.o mvt.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3

tile-join: tile-join.o projection.o pool.o mbtiles.o mvt.o memfile.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

unit: unit.o text.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

-include $(wildcard *.d)

%.o: %.c
	$(CC) -MMD $(PG) $(INCLUDES) $(FINAL_FLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) -MMD $(PG) $(INCLUDES) $(FINAL_FLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f tippecanoe *.o *.d */*.o */*.d

indent:
	clang-format -i -style="{BasedOnStyle: Google, IndentWidth: 8, UseTab: Always, AllowShortIfStatementsOnASingleLine: false, ColumnLimit: 0, ContinuationIndentWidth: 8, SpaceAfterCStyleCast: true, IndentCaseLabels: false, AllowShortBlocksOnASingleLine: false, AllowShortFunctionsOnASingleLine: false, SortIncludes: false}" $(C) $(H)

TESTS = $(wildcard tests/*/out/*.json)
SPACE = $(NULL) $(NULL)

test: tippecanoe tippecanoe-decode $(addsuffix .check,$(TESTS)) parallel-test pbf-test join-test enumerate-test unit
	./unit

# Work around Makefile and filename punctuation limits: _ for space, @ for :, % for /
%.json.check:
	./tippecanoe -ad -f -o $@.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json.check,%,$(word 4,$(subst /, ,$@)))))) $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json) < /dev/null
	./tippecanoe-decode $@.mbtiles > $@.out
	cmp $(patsubst %.check,%,$@) $@.out
	rm $@.out $@.mbtiles

parallel-test:
	mkdir -p tests/parallel
	perl -e 'for ($$i = 0; $$i < 20; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; print "{ \"type\": \"Feature\", \"properties\": { \"yes\": \"no\", \"who\": 1 }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in1.json
	perl -e 'for ($$i = 0; $$i < 300000; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; print "{ \"type\": \"Feature\", \"properties\": { }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in2.json
	perl -e 'for ($$i = 0; $$i < 20; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; print "{ \"type\": \"Feature\", \"properties\": { }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in3.json
	echo -n "" > tests/parallel/empty1.json
	echo "" > tests/parallel/empty2.json
	./tippecanoe -z5 -f -pi -l test -n test -o tests/parallel/linear-file.mbtiles tests/parallel/in[123].json tests/parallel/empty[12].json
	./tippecanoe -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-file.mbtiles tests/parallel/in[123].json tests/parallel/empty[12].json
	cat tests/parallel/in[123].json | ./tippecanoe -z5 -f -pi -l test -n test -o tests/parallel/linear-pipe.mbtiles
	cat tests/parallel/in[123].json | ./tippecanoe -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-pipe.mbtiles
	./tippecanoe -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-pipes.mbtiles <(cat tests/parallel/in1.json) <(cat tests/parallel/empty1.json) <(cat tests/parallel/empty2.json) <(cat tests/parallel/in2.json) /dev/null <(cat tests/parallel/in3.json)
	./tippecanoe-decode tests/parallel/linear-file.mbtiles > tests/parallel/linear-file.json
	./tippecanoe-decode tests/parallel/parallel-file.mbtiles > tests/parallel/parallel-file.json
	./tippecanoe-decode tests/parallel/linear-pipe.mbtiles > tests/parallel/linear-pipe.json
	./tippecanoe-decode tests/parallel/parallel-pipe.mbtiles > tests/parallel/parallel-pipe.json
	./tippecanoe-decode tests/parallel/parallel-pipes.mbtiles > tests/parallel/parallel-pipes.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-file.json
	cmp tests/parallel/linear-file.json tests/parallel/linear-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-pipes.json
	rm tests/parallel/*.mbtiles tests/parallel/*.json

pbf-test:
	./tippecanoe-decode tests/pbf/11-328-791.vector.pbf 11 328 791 > tests/pbf/11-328-791.vector.pbf.out
	cmp tests/pbf/11-328-791.json tests/pbf/11-328-791.vector.pbf.out
	rm tests/pbf/11-328-791.vector.pbf.out
	./tippecanoe-decode -t EPSG:3857 tests/pbf/11-328-791.vector.pbf 11 328 791 > tests/pbf/11-328-791.3857.vector.pbf.out
	cmp tests/pbf/11-328-791.3857.json tests/pbf/11-328-791.3857.vector.pbf.out
	rm tests/pbf/11-328-791.3857.vector.pbf.out

enumerate-test:
	./tippecanoe -z5 -f -o tests/ne_110m_admin_0_countries/out/enum.mbtiles tests/ne_110m_admin_0_countries/in.json
	./tippecanoe-enumerate tests/ne_110m_admin_0_countries/out/enum.mbtiles > tests/ne_110m_admin_0_countries/out/enum.check
	cmp tests/ne_110m_admin_0_countries/out/enum tests/ne_110m_admin_0_countries/out/enum.check
	rm tests/ne_110m_admin_0_countries/out/enum.mbtiles tests/ne_110m_admin_0_countries/out/enum.check

join-test:
	./tippecanoe -f -z12 -o tests/join-population/tabblock_06001420.mbtiles tests/join-population/tabblock_06001420.json
	./tippecanoe -f -z12 -o tests/join-population/tabblock_06001420.mbtiles tests/join-population/tabblock_06001420.json
	./tippecanoe -f -Z5 -z10 -o tests/join-population/macarthur.mbtiles -l macarthur tests/join-population/macarthur.json
	./tippecanoe -f -d10 -D10 -Z9 -z11 -o tests/join-population/macarthur2.mbtiles -l macarthur tests/join-population/macarthur2.json
	./tile-join -f -o tests/join-population/joined.mbtiles -x GEOID10 -c tests/join-population/population.csv tests/join-population/tabblock_06001420.mbtiles
	./tile-join -f -i -o tests/join-population/joined-i.mbtiles -x GEOID10 -c tests/join-population/population.csv tests/join-population/tabblock_06001420.mbtiles
	./tile-join -f -o tests/join-population/merged.mbtiles tests/join-population/tabblock_06001420.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	./tippecanoe-decode tests/join-population/joined.mbtiles > tests/join-population/joined.mbtiles.json.check
	./tippecanoe-decode tests/join-population/joined-i.mbtiles > tests/join-population/joined-i.mbtiles.json.check
	./tippecanoe-decode tests/join-population/merged.mbtiles > tests/join-population/merged.mbtiles.json.check
	cmp tests/join-population/joined.mbtiles.json.check tests/join-population/joined.mbtiles.json
	cmp tests/join-population/joined-i.mbtiles.json.check tests/join-population/joined-i.mbtiles.json
	cmp tests/join-population/merged.mbtiles.json.check tests/join-population/merged.mbtiles.json
	rm tests/join-population/tabblock_06001420.mbtiles tests/join-population/joined.mbtiles tests/join-population/joined-i.mbtiles tests/join-population/joined.mbtiles.json.check tests/join-population/joined-i.mbtiles.json.check tests/join-population/macarthur.mbtiles tests/join-population/merged.mbtiles tests/join-population/merged.mbtiles.json.check tests/join-population/macarthur2.mbtiles

# Use this target to regenerate the standards that the tests are compared against
# after making a change that legitimately changes their output

prep-test: $(TESTS)

tests/%.json: Makefile tippecanoe tippecanoe-decode
	./tippecanoe -f -o $@.check.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json,%,$(word 4,$(subst /, ,$@)))))) $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json)
	./tippecanoe-decode $@.check.mbtiles > $@
	cmp $(patsubst %.check,%,$@) $@
	rm $@.check.mbtiles
