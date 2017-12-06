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
WARNING_FLAGS := -Wall -Wshadow -Wsign-compare -Wextra -Wunreachable-code -Wuninitialized -Wshadow
RELEASE_FLAGS := -O3 -DNDEBUG
DEBUG_FLAGS := -O0 -DDEBUG -fno-inline-functions -fno-omit-frame-pointer

ifeq ($(BUILDTYPE),Release)
	FINAL_FLAGS := -g $(WARNING_FLAGS) $(RELEASE_FLAGS)
else
	FINAL_FLAGS := -g $(WARNING_FLAGS) $(DEBUG_FLAGS)
endif

all: tippecanoe tippecanoe-enumerate tippecanoe-decode tile-join unit tippecanoe-json-tool

docs: man/tippecanoe.1

install: tippecanoe tippecanoe-enumerate tippecanoe-decode tile-join tippecanoe-json-tool
	mkdir -p $(PREFIX)/bin
	mkdir -p $(MANDIR)
	cp tippecanoe $(PREFIX)/bin/tippecanoe
	cp tippecanoe-enumerate $(PREFIX)/bin/tippecanoe-enumerate
	cp tippecanoe-decode $(PREFIX)/bin/tippecanoe-decode
	cp tippecanoe-json-tool $(PREFIX)/bin/tippecanoe-json-tool
	cp tile-join $(PREFIX)/bin/tile-join
	cp man/tippecanoe.1 $(MANDIR)/tippecanoe.1

uninstall:
	rm $(PREFIX)/bin/tippecanoe $(PREFIX)/bin/tippecanoe-enumerate $(PREFIX)/bin/tippecanoe-decode $(PREFIX)/bin/tile-join $(MANDIR)/tippecanoe.1 $(PREFIX)/bin/tippecanoe-json-tool

man/tippecanoe.1: README.md
	md2man-roff README.md > man/tippecanoe.1

PG=

H = $(wildcard *.h) $(wildcard *.hpp)
C = $(wildcard *.c) $(wildcard *.cpp)

INCLUDES = -I/usr/local/include -I.
LIBS = -L/usr/local/lib

tippecanoe: geojson.o jsonpull/jsonpull.o tile.o pool.o mbtiles.o geometry.o projection.o memfile.o mvt.o serial.o main.o text.o dirtiles.o plugin.o read_json.o write_json.o geobuf.o shapefile.o evaluator.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

tippecanoe-enumerate: enumerate.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lsqlite3

tippecanoe-decode: decode.o projection.o mvt.o write_json.o text.o jsonpull/jsonpull.o dirtiles.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3

tile-join: tile-join.o projection.o pool.o mbtiles.o mvt.o memfile.o dirtiles.o jsonpull/jsonpull.o text.o evaluator.o csv.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

tippecanoe-json-tool: jsontool.o jsonpull/jsonpull.o csv.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

unit: unit.o text.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

-include $(wildcard *.d)

%.o: %.c
	$(CC) -MMD $(PG) $(INCLUDES) $(FINAL_FLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) -MMD $(PG) $(INCLUDES) $(FINAL_FLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f ./tippecanoe ./tippecanoe-* ./tile-join ./unit *.o *.d */*.o */*.d

indent:
	clang-format -i -style="{BasedOnStyle: Google, IndentWidth: 8, UseTab: Always, AllowShortIfStatementsOnASingleLine: false, ColumnLimit: 0, ContinuationIndentWidth: 8, SpaceAfterCStyleCast: true, IndentCaseLabels: false, AllowShortBlocksOnASingleLine: false, AllowShortFunctionsOnASingleLine: false, SortIncludes: false}" $(C) $(H)

TESTS = $(wildcard tests/*/out/*.json)
SPACE = $(NULL) $(NULL)

test: tippecanoe tippecanoe-decode $(addsuffix .check,$(TESTS)) raw-tiles-test parallel-test pbf-test join-test enumerate-test decode-test join-filter-test unit json-tool-test allow-existing-test
	./unit

# Work around Makefile and filename punctuation limits: _ for space, @ for :, % for /
%.json.check:
	./tippecanoe -aD -f -o $@.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json.check,%,$(word 4,$(subst /, ,$@)))))) $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json) < /dev/null
	./tippecanoe-decode $@.mbtiles > $@.out
	cmp $@.out $(patsubst %.check,%,$@)
	rm $@.out $@.mbtiles

# Don't test overflow with geobuf, because it fails (https://github.com/mapbox/geobuf/issues/87)
geobuf-test: tippecanoe-json-tool $(addsuffix .checkbuf,$(filter-out tests/overflow/out/-z0.json,$(TESTS)))

# For quicker address sanitizer build, hope that regular JSON parsing is tested enough by parallel and join tests
fewer-tests: tippecanoe tippecanoe-decode geobuf-test raw-tiles-test parallel-test pbf-test join-test enumerate-test decode-test join-filter-test unit

# XXX Use proper makefile rules instead of a for loop
%.json.checkbuf:
	for i in $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json); do ./tippecanoe-json-tool -w $$i | ./node_modules/geobuf/bin/json2geobuf > $$i.geobuf; done
	./tippecanoe -aD -f -o $@.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json.checkbuf,%,$(word 4,$(subst /, ,$@)))))) $(addsuffix .geobuf,$(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json)) < /dev/null
	./tippecanoe-decode $@.mbtiles | sed 's/checkbuf/check/g' > $@.out
	cmp $@.out $(patsubst %.checkbuf,%,$@)
	rm $@.out $@.mbtiles

parallel-test:
	mkdir -p tests/parallel
	perl -e 'for ($$i = 0; $$i < 20; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; $$k = rand(1); $$v = rand(1); print "{ \"type\": \"Feature\", \"properties\": { \"yes\": \"no\", \"who\": 1, \"$$k\": \"$$v\" }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in1.json
	perl -e 'for ($$i = 0; $$i < 300000; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; print "{ \"type\": \"Feature\", \"properties\": { }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in2.json
	perl -e 'for ($$i = 0; $$i < 20; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; print "{ \"type\": \"Feature\", \"properties\": { }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in3.json
	perl -e 'for ($$i = 0; $$i < 20; $$i++) { $$lon = rand(360) - 180; $$lat = rand(180) - 90; $$v = rand(1); print "{ \"type\": \"Feature\", \"properties\": { }, \"tippecanoe\": { \"layer\": \"$$v\" }, \"geometry\": { \"type\": \"Point\", \"coordinates\": [ $$lon, $$lat ] } }\n"; }' > tests/parallel/in4.json
	echo -n "" > tests/parallel/empty1.json
	echo "" > tests/parallel/empty2.json
	./tippecanoe -z5 -f -pi -l test -n test -o tests/parallel/linear-file.mbtiles tests/parallel/in[1234].json tests/parallel/empty[12].json
	./tippecanoe -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-file.mbtiles tests/parallel/in[1234].json tests/parallel/empty[12].json
	cat tests/parallel/in[1234].json | ./tippecanoe -z5 -f -pi -l test -n test -o tests/parallel/linear-pipe.mbtiles
	cat tests/parallel/in[1234].json | ./tippecanoe -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-pipe.mbtiles
	cat tests/parallel/in[1234].json | sed 's/^/@/' | tr '@' '\036' | ./tippecanoe -z5 -f -pi -l test -n test -o tests/parallel/implicit-pipe.mbtiles
	./tippecanoe -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-pipes.mbtiles <(cat tests/parallel/in1.json) <(cat tests/parallel/empty1.json) <(cat tests/parallel/empty2.json) <(cat tests/parallel/in2.json) /dev/null <(cat tests/parallel/in3.json) <(cat tests/parallel/in4.json)
	./tippecanoe-decode tests/parallel/linear-file.mbtiles > tests/parallel/linear-file.json
	./tippecanoe-decode tests/parallel/parallel-file.mbtiles > tests/parallel/parallel-file.json
	./tippecanoe-decode tests/parallel/linear-pipe.mbtiles > tests/parallel/linear-pipe.json
	./tippecanoe-decode tests/parallel/parallel-pipe.mbtiles > tests/parallel/parallel-pipe.json
	./tippecanoe-decode tests/parallel/implicit-pipe.mbtiles > tests/parallel/implicit-pipe.json
	./tippecanoe-decode tests/parallel/parallel-pipes.mbtiles > tests/parallel/parallel-pipes.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-file.json
	cmp tests/parallel/linear-file.json tests/parallel/linear-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/implicit-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-pipes.json
	rm tests/parallel/*.mbtiles tests/parallel/*.json

raw-tiles-test:	
	./tippecanoe -f -e tests/raw-tiles/raw-tiles tests/raw-tiles/hackspots.geojson -pC
	diff -x '*.DS_Store' -rq tests/raw-tiles/raw-tiles tests/raw-tiles/compare
	rm -rf tests/raw-tiles/raw-tiles

decode-test:
	mkdir -p tests/muni/decode
	./tippecanoe -z11 -Z11 -f -o tests/muni/decode/multi.mbtiles tests/muni/*.json
	./tippecanoe-decode -l subway tests/muni/decode/multi.mbtiles > tests/muni/decode/multi.mbtiles.json.check
	./tippecanoe-decode -c tests/muni/decode/multi.mbtiles > tests/muni/decode/multi.mbtiles.pipeline.json.check
	./tippecanoe-decode --stats tests/muni/decode/multi.mbtiles > tests/muni/decode/multi.mbtiles.stats.json.check
	cmp tests/muni/decode/multi.mbtiles.json.check tests/muni/decode/multi.mbtiles.json
	cmp tests/muni/decode/multi.mbtiles.pipeline.json.check tests/muni/decode/multi.mbtiles.pipeline.json
	cmp tests/muni/decode/multi.mbtiles.stats.json.check tests/muni/decode/multi.mbtiles.stats.json
	rm -f tests/muni/decode/multi.mbtiles.json.check tests/muni/decode/multi.mbtiles tests/muni/decode/multi.mbtiles.pipeline.json.check tests/muni/decode/multi.mbtiles.stats.json.check

pbf-test:
	./tippecanoe-decode tests/pbf/11-328-791.vector.pbf 11 328 791 > tests/pbf/11-328-791.vector.pbf.out
	cmp tests/pbf/11-328-791.json tests/pbf/11-328-791.vector.pbf.out
	rm tests/pbf/11-328-791.vector.pbf.out
	./tippecanoe-decode -s EPSG:3857 tests/pbf/11-328-791.vector.pbf 11 328 791 > tests/pbf/11-328-791.3857.vector.pbf.out
	cmp tests/pbf/11-328-791.3857.json tests/pbf/11-328-791.3857.vector.pbf.out
	rm tests/pbf/11-328-791.3857.vector.pbf.out

enumerate-test:
	./tippecanoe -z5 -f -o tests/ne_110m_admin_0_countries/out/enum.mbtiles tests/ne_110m_admin_0_countries/in.json
	./tippecanoe-enumerate tests/ne_110m_admin_0_countries/out/enum.mbtiles > tests/ne_110m_admin_0_countries/out/enum.check
	cmp tests/ne_110m_admin_0_countries/out/enum tests/ne_110m_admin_0_countries/out/enum.check
	rm tests/ne_110m_admin_0_countries/out/enum.mbtiles tests/ne_110m_admin_0_countries/out/enum.check

join-test:
	./tippecanoe -f -z12 -o tests/join-population/tabblock_06001420.mbtiles tests/join-population/tabblock_06001420.json
	./tippecanoe -f -Z5 -z10 -o tests/join-population/macarthur.mbtiles -l macarthur tests/join-population/macarthur.json
	./tile-join -f -Z6 -z9 -o tests/join-population/macarthur-6-9.mbtiles tests/join-population/macarthur.mbtiles
	./tippecanoe-decode tests/join-population/macarthur-6-9.mbtiles > tests/join-population/macarthur-6-9.mbtiles.json.check
	cmp tests/join-population/macarthur-6-9.mbtiles.json.check tests/join-population/macarthur-6-9.mbtiles.json
	rm -f tests/join-population/macarthur-6-9.mbtiles.json.check tests/join-population/macarthur-6-9.mbtiles
	./tippecanoe -f -d10 -D10 -Z9 -z11 -o tests/join-population/macarthur2.mbtiles -l macarthur tests/join-population/macarthur2.json
	./tile-join --quiet --force -o tests/join-population/joined.mbtiles -x GEOID10 -c tests/join-population/population.csv tests/join-population/tabblock_06001420.mbtiles
	./tile-join --quiet --force --no-tile-stats -o tests/join-population/joined-no-tile-stats.mbtiles -x GEOID10 -c tests/join-population/population.csv tests/join-population/tabblock_06001420.mbtiles
	./tile-join -f -i -o tests/join-population/joined-i.mbtiles -x GEOID10 -c tests/join-population/population.csv tests/join-population/tabblock_06001420.mbtiles
	./tile-join -f -o tests/join-population/merged.mbtiles tests/join-population/tabblock_06001420.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	./tile-join -f -c tests/join-population/windows.csv -o tests/join-population/windows.mbtiles tests/join-population/macarthur.mbtiles
	./tippecanoe-decode --maximum-zoom=11 --minimum-zoom=4 tests/join-population/joined.mbtiles > tests/join-population/joined.mbtiles.json.check
	./tippecanoe-decode --maximum-zoom=11 --minimum-zoom=4 tests/join-population/joined-no-tile-stats.mbtiles > tests/join-population/joined-no-tile-stats.mbtiles.json.check
	./tippecanoe-decode tests/join-population/joined-i.mbtiles > tests/join-population/joined-i.mbtiles.json.check
	./tippecanoe-decode tests/join-population/merged.mbtiles > tests/join-population/merged.mbtiles.json.check
	./tippecanoe-decode tests/join-population/windows.mbtiles > tests/join-population/windows.mbtiles.json.check
	cmp tests/join-population/joined.mbtiles.json.check tests/join-population/joined.mbtiles.json
	cmp tests/join-population/joined-no-tile-stats.mbtiles.json.check tests/join-population/joined-no-tile-stats.mbtiles.json
	cmp tests/join-population/joined-i.mbtiles.json.check tests/join-population/joined-i.mbtiles.json
	cmp tests/join-population/merged.mbtiles.json.check tests/join-population/merged.mbtiles.json
	cmp tests/join-population/windows.mbtiles.json.check tests/join-population/windows.mbtiles.json
	./tile-join -f -l macarthur -n "macarthur name" -N "macarthur description" -A "macarthur attribution" -o tests/join-population/just-macarthur.mbtiles tests/join-population/merged.mbtiles
	./tile-join -f -L macarthur -o tests/join-population/no-macarthur.mbtiles tests/join-population/merged.mbtiles
	./tippecanoe-decode tests/join-population/just-macarthur.mbtiles > tests/join-population/just-macarthur.mbtiles.json.check
	./tippecanoe-decode tests/join-population/no-macarthur.mbtiles > tests/join-population/no-macarthur.mbtiles.json.check
	cmp tests/join-population/just-macarthur.mbtiles.json.check tests/join-population/just-macarthur.mbtiles.json
	cmp tests/join-population/no-macarthur.mbtiles.json.check tests/join-population/no-macarthur.mbtiles.json	
	./tile-join --no-tile-compression -f -e tests/join-population/raw-merged-folder tests/join-population/tabblock_06001420.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	diff -x '*.DS_Store' -rq tests/join-population/raw-merged-folder tests/join-population/raw-merged-folder-compare
	./tippecanoe -z12 -f -e tests/join-population/tabblock_06001420-folder tests/join-population/tabblock_06001420.json
	./tippecanoe -Z5 -z10 -f -e tests/join-population/macarthur-folder -l macarthur tests/join-population/macarthur.json
	./tippecanoe -d10 -D10 -Z9 -z11 -f -e tests/join-population/macarthur2-folder -l macarthur tests/join-population/macarthur2.json
	./tile-join -f -o tests/join-population/merged-folder.mbtiles tests/join-population/tabblock_06001420-folder tests/join-population/macarthur-folder tests/join-population/macarthur2-folder
	./tippecanoe-decode tests/join-population/merged-folder.mbtiles > tests/join-population/merged-folder.mbtiles.json.check
	cmp tests/join-population/merged-folder.mbtiles.json.check tests/join-population/merged-folder.mbtiles.json
	./tile-join -n "merged name" -N "merged description" -f -e tests/join-population/merged-mbtiles-to-folder tests/join-population/tabblock_06001420.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	./tile-join -n "merged name" -N "merged description" -f -e tests/join-population/merged-folders-to-folder tests/join-population/tabblock_06001420-folder tests/join-population/macarthur-folder tests/join-population/macarthur2-folder
	diff -x '*.DS_Store' -rq tests/join-population/merged-mbtiles-to-folder tests/join-population/merged-folders-to-folder
	./tile-join -f -c tests/join-population/windows.csv -o tests/join-population/windows-merged.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2-folder
	./tile-join -c tests/join-population/windows.csv -f -e tests/join-population/windows-merged-folder tests/join-population/macarthur.mbtiles tests/join-population/macarthur2-folder
	./tile-join -f -o tests/join-population/windows-merged2.mbtiles tests/join-population/windows-merged-folder
	./tippecanoe-decode tests/join-population/windows-merged.mbtiles > tests/join-population/windows-merged.mbtiles.json.check
	./tippecanoe-decode tests/join-population/windows-merged2.mbtiles > tests/join-population/windows-merged2.mbtiles.json.check
	cmp tests/join-population/windows-merged.mbtiles.json.check tests/join-population/windows-merged2.mbtiles.json.check
	./tile-join -f -o tests/join-population/macarthur-and-macarthur2-merged.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2-folder
	./tile-join -f -e tests/join-population/macarthur-and-macarthur2-folder tests/join-population/macarthur.mbtiles tests/join-population/macarthur2-folder
	./tile-join -f -o tests/join-population/macarthur-and-macarthur2-merged2.mbtiles tests/join-population/macarthur-and-macarthur2-folder
	./tippecanoe-decode tests/join-population/macarthur-and-macarthur2-merged.mbtiles > tests/join-population/macarthur-and-macarthur2-merged.mbtiles.json.check
	./tippecanoe-decode tests/join-population/macarthur-and-macarthur2-merged2.mbtiles > tests/join-population/macarthur-and-macarthur2-merged2.mbtiles.json.check
	cmp tests/join-population/macarthur-and-macarthur2-merged.mbtiles.json.check tests/join-population/macarthur-and-macarthur2-merged2.mbtiles.json.check
	rm tests/join-population/tabblock_06001420.mbtiles tests/join-population/joined.mbtiles tests/join-population/joined-i.mbtiles tests/join-population/joined.mbtiles.json.check tests/join-population/joined-i.mbtiles.json.check tests/join-population/macarthur.mbtiles tests/join-population/merged.mbtiles tests/join-population/merged.mbtiles.json.check  tests/join-population/merged-folder.mbtiles tests/join-population/macarthur2.mbtiles tests/join-population/windows.mbtiles tests/join-population/windows-merged.mbtiles tests/join-population/windows-merged2.mbtiles tests/join-population/windows.mbtiles.json.check tests/join-population/just-macarthur.mbtiles tests/join-population/no-macarthur.mbtiles tests/join-population/just-macarthur.mbtiles.json.check tests/join-population/no-macarthur.mbtiles.json.check tests/join-population/merged-folder.mbtiles.json.check tests/join-population/windows-merged.mbtiles.json.check tests/join-population/windows-merged2.mbtiles.json.check tests/join-population/macarthur-and-macarthur2-merged.mbtiles tests/join-population/macarthur-and-macarthur2-merged2.mbtiles tests/join-population/macarthur-and-macarthur2-merged.mbtiles.json.check tests/join-population/macarthur-and-macarthur2-merged2.mbtiles.json.check
	rm -rf tests/join-population/raw-merged-folder tests/join-population/tabblock_06001420-folder tests/join-population/macarthur-folder tests/join-population/macarthur2-folder tests/join-population/merged-mbtiles-to-folder tests/join-population/merged-folders-to-folder tests/join-population/windows-merged-folder tests/join-population/macarthur-and-macarthur2-folder
	# Test renaming of layers
	./tippecanoe -f -Z5 -z10 -o tests/join-population/macarthur.mbtiles -l macarthur1 tests/join-population/macarthur.json
	./tippecanoe -f -Z5 -z10 -o tests/join-population/macarthur2.mbtiles -l macarthur2 tests/join-population/macarthur2.json
	./tile-join -R macarthur1:one --rename-layer=macarthur2:two -f -o tests/join-population/renamed.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	./tippecanoe-decode tests/join-population/renamed.mbtiles > tests/join-population/renamed.mbtiles.json.check
	cmp tests/join-population/renamed.mbtiles.json.check tests/join-population/renamed.mbtiles.json
	rm -f tests/join-population/renamed.mbtiles.json.check tests/join-population/renamed.mbtiles.json.check tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles

join-filter-test:
	# Comes out different from the direct tippecanoe run because null attributes are lost
	./tippecanoe -z0 -f -o tests/feature-filter/out/all.mbtiles tests/feature-filter/in.json
	./tile-join -J tests/feature-filter/filter -f -o tests/feature-filter/out/filtered.mbtiles tests/feature-filter/out/all.mbtiles
	./tippecanoe-decode tests/feature-filter/out/filtered.mbtiles > tests/feature-filter/out/filtered.json.check
	cmp tests/feature-filter/out/filtered.json.check tests/feature-filter/out/filtered.json.standard
	rm -f tests/feature-filter/out/filtered.json.check tests/feature-filter/out/filtered.mbtiles tests/feature-filter/out/all.mbtiles

json-tool-test: tippecanoe-json-tool
	./tippecanoe-json-tool -e GEOID10 tests/join-population/tabblock_06001420.json | sort > tests/join-population/tabblock_06001420.json.sort
	./tippecanoe-json-tool -c tests/join-population/population.csv tests/join-population/tabblock_06001420.json.sort > tests/join-population/tabblock_06001420.json.sort.joined
	cmp tests/join-population/tabblock_06001420.json.sort.joined tests/join-population/tabblock_06001420.json.sort.joined.standard
	rm -f tests/join-population/tabblock_06001420.json.sort tests/join-population/tabblock_06001420.json.sort.joined

allow-existing-test:
	# Make a tileset
	./tippecanoe -Z0 -z0 -f -o tests/allow-existing/both.mbtiles tests/coalesce-tract/tl_2010_06001_tract10.json
	# Writing to existing should fail
	if ./tippecanoe -Z1 -z1 -o tests/allow-existing/both.mbtiles tests/coalesce-tract/tl_2010_06001_tract10.json; then exit 1; else exit 0; fi
	# Replace existing
	./tippecanoe -Z8 -z9 -f -o tests/allow-existing/both.mbtiles tests/coalesce-tract/tl_2010_06001_tract10.json
	./tippecanoe -Z10 -z11 -F -o tests/allow-existing/both.mbtiles tests/coalesce-tract/tl_2010_06001_tract10.json
	./tippecanoe-decode tests/allow-existing/both.mbtiles > tests/allow-existing/both.mbtiles.json.check
	cmp tests/allow-existing/both.mbtiles.json.check tests/allow-existing/both.mbtiles.json
	# Make a tileset
	./tippecanoe -Z0 -z0 -f -e tests/allow-existing/both.dir tests/coalesce-tract/tl_2010_06001_tract10.json
	# Writing to existing should fail
	if ./tippecanoe -Z1 -z1 -e tests/allow-existing/both.dir tests/coalesce-tract/tl_2010_06001_tract10.json; then exit 1; else exit 0; fi
	# Replace existing
	./tippecanoe -Z8 -z9 -f -e tests/allow-existing/both.dir tests/coalesce-tract/tl_2010_06001_tract10.json
	./tippecanoe -Z10 -z11 -F -e tests/allow-existing/both.dir tests/coalesce-tract/tl_2010_06001_tract10.json
	./tippecanoe-decode tests/allow-existing/both.dir | sed 's/both\.dir/both.mbtiles/g' > tests/allow-existing/both.dir.json.check
	cmp tests/allow-existing/both.dir.json.check tests/allow-existing/both.mbtiles.json
	rm -r tests/allow-existing/both.dir.json.check tests/allow-existing/both.dir tests/allow-existing/both.mbtiles.json.check tests/allow-existing/both.mbtiles

# Use this target to regenerate the standards that the tests are compared against
# after making a change that legitimately changes their output

prep-test: $(TESTS)

tests/%.json: Makefile tippecanoe tippecanoe-decode
	./tippecanoe -f -o $@.check.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json,%,$(word 4,$(subst /, ,$@)))))) $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json)
	./tippecanoe-decode $@.check.mbtiles > $@
	cmp $(patsubst %.check,%,$@) $@
	rm $@.check.mbtiles
