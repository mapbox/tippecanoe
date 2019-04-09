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

tippecanoe: geojson.o jsonpull/jsonpull.o tile.o pool.o mbtiles.o geometry.o projection.o memfile.o mvt.o serial.o main.o text.o dirtiles.o plugin.o read_json.o write_json.o geobuf.o evaluator.o geocsv.o csv.o geojson-loop.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

tippecanoe-enumerate: enumerate.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lsqlite3

tippecanoe-decode: decode.o projection.o mvt.o write_json.o text.o jsonpull/jsonpull.o dirtiles.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3

tile-join: tile-join.o projection.o pool.o mbtiles.o mvt.o memfile.o dirtiles.o jsonpull/jsonpull.o text.o evaluator.o csv.o write_json.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

tippecanoe-json-tool: jsontool.o jsonpull/jsonpull.o csv.o text.o geojson-loop.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

unit: unit.o text.o
	$(CXX) $(PG) $(LIBS) $(FINAL_FLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm -lz -lsqlite3 -lpthread

-include $(wildcard *.d)

%.o: %.c
	$(CC) -MMD $(PG) $(INCLUDES) $(FINAL_FLAGS) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) -MMD $(PG) $(INCLUDES) $(FINAL_FLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f ./tippecanoe ./tippecanoe-* ./tile-join ./unit *.o *.d */*.o */*.d tests/**/*.mbtiles tests/**/*.check

indent:
	clang-format -i -style="{BasedOnStyle: Google, IndentWidth: 8, UseTab: Always, AllowShortIfStatementsOnASingleLine: false, ColumnLimit: 0, ContinuationIndentWidth: 8, SpaceAfterCStyleCast: true, IndentCaseLabels: false, AllowShortBlocksOnASingleLine: false, AllowShortFunctionsOnASingleLine: false, SortIncludes: false}" $(C) $(H)

TESTS = $(wildcard tests/*/out/*.json)
SPACE = $(NULL) $(NULL)

test: tippecanoe tippecanoe-decode $(addsuffix .check,$(TESTS)) raw-tiles-test parallel-test pbf-test join-test enumerate-test decode-test join-filter-test unit json-tool-test allow-existing-test csv-test layer-json-test
	./unit

suffixes = json json.gz

# Work around Makefile and filename punctuation limits: _ for space, @ for :, % for /
%.json.check:
	./tippecanoe -q -a@ -f -o $@.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json.check,%,$(word 4,$(subst /, ,$@)))))) $(foreach suffix,$(suffixes),$(sort $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.$(suffix)))) < /dev/null
	./tippecanoe-decode -x generator $@.mbtiles > $@.out
	cmp $@.out $(patsubst %.check,%,$@)
	rm $@.out $@.mbtiles

# Don't test overflow with geobuf, because it fails (https://github.com/mapbox/geobuf/issues/87)
# Don't test stringids with geobuf, because it fails
nogeobuf = tests/overflow/out/-z0.json $(wildcard tests/stringid/out/*.json)
geobuf-test: tippecanoe-json-tool $(addsuffix .checkbuf,$(filter-out $(nogeobuf),$(TESTS)))

# For quicker address sanitizer build, hope that regular JSON parsing is tested enough by parallel and join tests
fewer-tests: tippecanoe tippecanoe-decode geobuf-test raw-tiles-test parallel-test pbf-test join-test enumerate-test decode-test join-filter-test unit

# XXX Use proper makefile rules instead of a for loop
%.json.checkbuf:
	for i in $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json); do ./tippecanoe-json-tool -w $$i | ./node_modules/geobuf/bin/json2geobuf > $$i.geobuf; done
	for i in $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.json.gz); do gzip -dc $$i | ./tippecanoe-json-tool -w | ./node_modules/geobuf/bin/json2geobuf > $$i.geobuf; done
	./tippecanoe -q -a@ -f -o $@.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json.checkbuf,%,$(word 4,$(subst /, ,$@)))))) $(foreach suffix,$(suffixes),$(addsuffix .geobuf,$(sort $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.$(suffix))))) < /dev/null
	./tippecanoe-decode -x generator $@.mbtiles | sed 's/checkbuf/check/g' | sed 's/\.geobuf//g' > $@.out
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
	./tippecanoe -q -z5 -f -pi -l test -n test -o tests/parallel/linear-file.mbtiles tests/parallel/in[1234].json tests/parallel/empty[12].json
	./tippecanoe -q -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-file.mbtiles tests/parallel/in[1234].json tests/parallel/empty[12].json
	cat tests/parallel/in[1234].json | ./tippecanoe -q -z5 -f -pi -l test -n test -o tests/parallel/linear-pipe.mbtiles
	cat tests/parallel/in[1234].json | ./tippecanoe -q -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-pipe.mbtiles
	cat tests/parallel/in[1234].json | sed 's/^/@/' | tr '@' '\036' | ./tippecanoe -q -z5 -f -pi -l test -n test -o tests/parallel/implicit-pipe.mbtiles
	./tippecanoe -q -z5 -f -pi -l test -n test -P -o tests/parallel/parallel-pipes.mbtiles <(cat tests/parallel/in1.json) <(cat tests/parallel/empty1.json) <(cat tests/parallel/empty2.json) <(cat tests/parallel/in2.json) /dev/null <(cat tests/parallel/in3.json) <(cat tests/parallel/in4.json)
	./tippecanoe-decode -x generator -x generator_options tests/parallel/linear-file.mbtiles > tests/parallel/linear-file.json
	./tippecanoe-decode -x generator -x generator_options tests/parallel/parallel-file.mbtiles > tests/parallel/parallel-file.json
	./tippecanoe-decode -x generator -x generator_options tests/parallel/linear-pipe.mbtiles > tests/parallel/linear-pipe.json
	./tippecanoe-decode -x generator -x generator_options tests/parallel/parallel-pipe.mbtiles > tests/parallel/parallel-pipe.json
	./tippecanoe-decode -x generator -x generator_options tests/parallel/implicit-pipe.mbtiles > tests/parallel/implicit-pipe.json
	./tippecanoe-decode -x generator -x generator_options tests/parallel/parallel-pipes.mbtiles > tests/parallel/parallel-pipes.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-file.json
	cmp tests/parallel/linear-file.json tests/parallel/linear-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/implicit-pipe.json
	cmp tests/parallel/linear-file.json tests/parallel/parallel-pipes.json
	rm tests/parallel/*.mbtiles tests/parallel/*.json

raw-tiles-test:
	./tippecanoe -q -f -e tests/raw-tiles/raw-tiles -r1 -pC tests/raw-tiles/hackspots.geojson
	./tippecanoe-decode -x generator tests/raw-tiles/raw-tiles > tests/raw-tiles/raw-tiles.json.check
	cmp tests/raw-tiles/raw-tiles.json.check tests/raw-tiles/raw-tiles.json
	# Test that -z and -Z work in tippecanoe-decode
	./tippecanoe-decode -x generator -Z6 -z7 tests/raw-tiles/raw-tiles > tests/raw-tiles/raw-tiles-z67.json.check
	cmp tests/raw-tiles/raw-tiles-z67.json.check tests/raw-tiles/raw-tiles-z67.json
	# Test that -z and -Z work in tile-join
	./tile-join -q -f -Z6 -z7 -e tests/raw-tiles/raw-tiles-z67 tests/raw-tiles/raw-tiles
	./tippecanoe-decode -x generator tests/raw-tiles/raw-tiles-z67 > tests/raw-tiles/raw-tiles-z67-join.json.check
	cmp tests/raw-tiles/raw-tiles-z67-join.json.check tests/raw-tiles/raw-tiles-z67-join.json
	rm -rf tests/raw-tiles/raw-tiles tests/raw-tiles/raw-tiles-z67 tests/raw-tiles/raw-tiles.json.check raw-tiles-z67.json.check tests/raw-tiles/raw-tiles-z67-join.json.check
	# Test that metadata.json is created even if all features are clipped away
	./tippecanoe -q -f -e tests/raw-tiles/nothing tests/raw-tiles/nothing.geojson
	./tippecanoe-decode -x generator tests/raw-tiles/nothing > tests/raw-tiles/nothing.json.check
	cmp tests/raw-tiles/nothing.json.check tests/raw-tiles/nothing.json
	rm -r tests/raw-tiles/nothing tests/raw-tiles/nothing.json.check

decode-test:
	mkdir -p tests/muni/decode
	./tippecanoe -q -z11 -Z11 -f -o tests/muni/decode/multi.mbtiles tests/muni/*.json
	./tippecanoe-decode -x generator -l subway tests/muni/decode/multi.mbtiles > tests/muni/decode/multi.mbtiles.json.check
	./tippecanoe-decode -x generator -c tests/muni/decode/multi.mbtiles > tests/muni/decode/multi.mbtiles.pipeline.json.check
	./tippecanoe-decode -x generator tests/muni/decode/multi.mbtiles 11 327 791 > tests/muni/decode/multi.mbtiles.onetile.json.check
	./tippecanoe-decode -x generator --stats tests/muni/decode/multi.mbtiles > tests/muni/decode/multi.mbtiles.stats.json.check
	cmp tests/muni/decode/multi.mbtiles.json.check tests/muni/decode/multi.mbtiles.json
	cmp tests/muni/decode/multi.mbtiles.pipeline.json.check tests/muni/decode/multi.mbtiles.pipeline.json
	cmp tests/muni/decode/multi.mbtiles.onetile.json.check tests/muni/decode/multi.mbtiles.onetile.json
	cmp tests/muni/decode/multi.mbtiles.stats.json.check tests/muni/decode/multi.mbtiles.stats.json
	rm -f tests/muni/decode/multi.mbtiles.json.check tests/muni/decode/multi.mbtiles tests/muni/decode/multi.mbtiles.pipeline.json.check tests/muni/decode/multi.mbtiles.stats.json.check tests/muni/decode/multi.mbtiles.onetile.json.check

pbf-test:
	./tippecanoe-decode -x generator tests/pbf/11-328-791.vector.pbf 11 328 791 > tests/pbf/11-328-791.vector.pbf.out
	cmp tests/pbf/11-328-791.json tests/pbf/11-328-791.vector.pbf.out
	rm tests/pbf/11-328-791.vector.pbf.out
	./tippecanoe-decode -x generator -s EPSG:3857 tests/pbf/11-328-791.vector.pbf 11 328 791 > tests/pbf/11-328-791.3857.vector.pbf.out
	cmp tests/pbf/11-328-791.3857.json tests/pbf/11-328-791.3857.vector.pbf.out
	rm tests/pbf/11-328-791.3857.vector.pbf.out

enumerate-test:
	./tippecanoe -q -z5 -f -o tests/ne_110m_admin_0_countries/out/enum.mbtiles tests/ne_110m_admin_0_countries/in.json.gz
	./tippecanoe-enumerate tests/ne_110m_admin_0_countries/out/enum.mbtiles > tests/ne_110m_admin_0_countries/out/enum.check
	cmp tests/ne_110m_admin_0_countries/out/enum tests/ne_110m_admin_0_countries/out/enum.check
	rm tests/ne_110m_admin_0_countries/out/enum.mbtiles tests/ne_110m_admin_0_countries/out/enum.check

join-test: tile-join
	./tippecanoe -q -f -z12 -o tests/join-population/tabblock_06001420.mbtiles -YALAND10:'Land area' -L'{"file": "tests/join-population/tabblock_06001420.json", "description": "population"}'
	./tippecanoe -q -f -Z5 -z10 -o tests/join-population/macarthur.mbtiles -l macarthur tests/join-population/macarthur.json
	./tile-join -q -f -Z6 -z9 -o tests/join-population/macarthur-6-9.mbtiles tests/join-population/macarthur.mbtiles
	./tippecanoe-decode -x generator tests/join-population/macarthur-6-9.mbtiles > tests/join-population/macarthur-6-9.mbtiles.json.check
	cmp tests/join-population/macarthur-6-9.mbtiles.json.check tests/join-population/macarthur-6-9.mbtiles.json
	./tile-join -q -f -Z6 -z9 -X -o tests/join-population/macarthur-6-9-exclude.mbtiles tests/join-population/macarthur.mbtiles
	./tippecanoe-decode -x generator tests/join-population/macarthur-6-9-exclude.mbtiles > tests/join-population/macarthur-6-9-exclude.mbtiles.json.check
	cmp tests/join-population/macarthur-6-9-exclude.mbtiles.json.check tests/join-population/macarthur-6-9-exclude.mbtiles.json
	rm -f tests/join-population/macarthur-6-9.mbtiles.json.check tests/join-population/macarthur-6-9.mbtiles tests/join-population/macarthur-6-9-exclude.mbtiles.json.check tests/join-population/macarthur-6-9-exclude.mbtiles
	./tippecanoe -q -f -d10 -D10 -Z9 -z11 -o tests/join-population/macarthur2.mbtiles -l macarthur tests/join-population/macarthur2.json
	./tile-join --quiet --force -o tests/join-population/joined.mbtiles -x GEOID10 -c tests/join-population/population.csv tests/join-population/tabblock_06001420.mbtiles
	./tile-join --quiet --force -o tests/join-population/joined-null.mbtiles --empty-csv-columns-are-null -x GEOID10 -c tests/join-population/population.csv tests/join-population/tabblock_06001420.mbtiles
	./tile-join --quiet --force --no-tile-stats -o tests/join-population/joined-no-tile-stats.mbtiles -x GEOID10 -c tests/join-population/population.csv tests/join-population/tabblock_06001420.mbtiles
	./tile-join -q -f -i -o tests/join-population/joined-i.mbtiles -x GEOID10 -c tests/join-population/population.csv tests/join-population/tabblock_06001420.mbtiles
	./tile-join -q -f -o tests/join-population/merged.mbtiles tests/join-population/tabblock_06001420.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	./tile-join -q -f -c tests/join-population/windows.csv -o tests/join-population/windows.mbtiles tests/join-population/macarthur.mbtiles
	./tippecanoe-decode -x generator --maximum-zoom=11 --minimum-zoom=4 tests/join-population/joined.mbtiles > tests/join-population/joined.mbtiles.json.check
	./tippecanoe-decode -x generator --maximum-zoom=11 --minimum-zoom=4 tests/join-population/joined-null.mbtiles > tests/join-population/joined-null.mbtiles.json.check
	./tippecanoe-decode -x generator --maximum-zoom=11 --minimum-zoom=4 tests/join-population/joined-no-tile-stats.mbtiles > tests/join-population/joined-no-tile-stats.mbtiles.json.check
	./tippecanoe-decode -x generator tests/join-population/joined-i.mbtiles > tests/join-population/joined-i.mbtiles.json.check
	./tippecanoe-decode -x generator tests/join-population/merged.mbtiles > tests/join-population/merged.mbtiles.json.check
	./tippecanoe-decode -x generator tests/join-population/windows.mbtiles > tests/join-population/windows.mbtiles.json.check
	cmp tests/join-population/joined.mbtiles.json.check tests/join-population/joined.mbtiles.json
	cmp tests/join-population/joined-null.mbtiles.json.check tests/join-population/joined-null.mbtiles.json
	cmp tests/join-population/joined-no-tile-stats.mbtiles.json.check tests/join-population/joined-no-tile-stats.mbtiles.json
	cmp tests/join-population/joined-i.mbtiles.json.check tests/join-population/joined-i.mbtiles.json
	cmp tests/join-population/merged.mbtiles.json.check tests/join-population/merged.mbtiles.json
	cmp tests/join-population/windows.mbtiles.json.check tests/join-population/windows.mbtiles.json
	rm -f tests/join-population/joined-null.mbtiles tests/join-population/joined-null.mbtiles.json.check
	./tile-join -q -f -l macarthur -n "macarthur name" -N "macarthur description" -A "macarthur's attribution" -o tests/join-population/just-macarthur.mbtiles tests/join-population/merged.mbtiles
	./tile-join -q -f -L macarthur -o tests/join-population/no-macarthur.mbtiles tests/join-population/merged.mbtiles
	./tippecanoe-decode -x generator tests/join-population/just-macarthur.mbtiles > tests/join-population/just-macarthur.mbtiles.json.check
	./tippecanoe-decode -x generator tests/join-population/no-macarthur.mbtiles > tests/join-population/no-macarthur.mbtiles.json.check
	cmp tests/join-population/just-macarthur.mbtiles.json.check tests/join-population/just-macarthur.mbtiles.json
	cmp tests/join-population/no-macarthur.mbtiles.json.check tests/join-population/no-macarthur.mbtiles.json
	./tile-join -q --no-tile-compression -f -e tests/join-population/raw-merged-folder tests/join-population/tabblock_06001420.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	./tippecanoe-decode -x generator tests/join-population/raw-merged-folder > tests/join-population/raw-merged-folder.json.check
	cmp tests/join-population/raw-merged-folder.json.check tests/join-population/raw-merged-folder.json
	rm -f tests/join-population/raw-merged-folder.json.check
	./tippecanoe -q -z12 -f -e tests/join-population/tabblock_06001420-folder -YALAND10:'Land area' -L'{"file": "tests/join-population/tabblock_06001420.json", "description": "population"}'
	./tippecanoe -q -Z5 -z10 -f -e tests/join-population/macarthur-folder -l macarthur tests/join-population/macarthur.json
	./tippecanoe -q -d10 -D10 -Z9 -z11 -f -e tests/join-population/macarthur2-folder -l macarthur tests/join-population/macarthur2.json
	./tile-join -q -f -o tests/join-population/merged-folder.mbtiles tests/join-population/tabblock_06001420-folder tests/join-population/macarthur-folder tests/join-population/macarthur2-folder
	./tippecanoe-decode -x generator tests/join-population/merged-folder.mbtiles > tests/join-population/merged-folder.mbtiles.json.check
	cmp tests/join-population/merged-folder.mbtiles.json.check tests/join-population/merged-folder.mbtiles.json
	./tile-join -q -n "merged name" -N "merged description" -f -e tests/join-population/merged-mbtiles-to-folder tests/join-population/tabblock_06001420.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	./tile-join -q -n "merged name" -N "merged description" -f -e tests/join-population/merged-folders-to-folder tests/join-population/tabblock_06001420-folder tests/join-population/macarthur-folder tests/join-population/macarthur2-folder
	./tippecanoe-decode -x generator -x generator_options tests/join-population/merged-mbtiles-to-folder > tests/join-population/merged-mbtiles-to-folder.json.check
	./tippecanoe-decode -x generator -x generator_options tests/join-population/merged-folders-to-folder > tests/join-population/merged-folders-to-folder.json.check
	cmp tests/join-population/merged-mbtiles-to-folder.json.check tests/join-population/merged-folders-to-folder.json.check
	rm -f tests/join-population/merged-mbtiles-to-folder.json.check tests/join-population/merged-folders-to-folder.json.check
	./tile-join -q -f -c tests/join-population/windows.csv -o tests/join-population/windows-merged.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2-folder
	./tile-join -q -c tests/join-population/windows.csv -f -e tests/join-population/windows-merged-folder tests/join-population/macarthur.mbtiles tests/join-population/macarthur2-folder
	./tile-join -q -f -o tests/join-population/windows-merged2.mbtiles tests/join-population/windows-merged-folder
	./tippecanoe-decode -x generator -x generator_options tests/join-population/windows-merged.mbtiles > tests/join-population/windows-merged.mbtiles.json.check
	./tippecanoe-decode -x generator -x generator_options tests/join-population/windows-merged2.mbtiles > tests/join-population/windows-merged2.mbtiles.json.check
	cmp tests/join-population/windows-merged.mbtiles.json.check tests/join-population/windows-merged2.mbtiles.json.check
	./tile-join -q -f -o tests/join-population/macarthur-and-macarthur2-merged.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2-folder
	./tile-join -q -f -e tests/join-population/macarthur-and-macarthur2-folder tests/join-population/macarthur.mbtiles tests/join-population/macarthur2-folder
	./tile-join -q -f -o tests/join-population/macarthur-and-macarthur2-merged2.mbtiles tests/join-population/macarthur-and-macarthur2-folder
	./tippecanoe-decode -x generator -x generator_options tests/join-population/macarthur-and-macarthur2-merged.mbtiles > tests/join-population/macarthur-and-macarthur2-merged.mbtiles.json.check
	./tippecanoe-decode -x generator -x generator_options tests/join-population/macarthur-and-macarthur2-merged2.mbtiles > tests/join-population/macarthur-and-macarthur2-merged2.mbtiles.json.check
	cmp tests/join-population/macarthur-and-macarthur2-merged.mbtiles.json.check tests/join-population/macarthur-and-macarthur2-merged2.mbtiles.json.check
	rm tests/join-population/tabblock_06001420.mbtiles tests/join-population/joined.mbtiles tests/join-population/joined-i.mbtiles tests/join-population/joined.mbtiles.json.check tests/join-population/joined-i.mbtiles.json.check tests/join-population/macarthur.mbtiles tests/join-population/merged.mbtiles tests/join-population/merged.mbtiles.json.check  tests/join-population/merged-folder.mbtiles tests/join-population/macarthur2.mbtiles tests/join-population/windows.mbtiles tests/join-population/windows-merged.mbtiles tests/join-population/windows-merged2.mbtiles tests/join-population/windows.mbtiles.json.check tests/join-population/just-macarthur.mbtiles tests/join-population/no-macarthur.mbtiles tests/join-population/just-macarthur.mbtiles.json.check tests/join-population/no-macarthur.mbtiles.json.check tests/join-population/merged-folder.mbtiles.json.check tests/join-population/windows-merged.mbtiles.json.check tests/join-population/windows-merged2.mbtiles.json.check tests/join-population/macarthur-and-macarthur2-merged.mbtiles tests/join-population/macarthur-and-macarthur2-merged2.mbtiles tests/join-population/macarthur-and-macarthur2-merged.mbtiles.json.check tests/join-population/macarthur-and-macarthur2-merged2.mbtiles.json.check
	rm -rf tests/join-population/raw-merged-folder tests/join-population/tabblock_06001420-folder tests/join-population/macarthur-folder tests/join-population/macarthur2-folder tests/join-population/merged-mbtiles-to-folder tests/join-population/merged-folders-to-folder tests/join-population/windows-merged-folder tests/join-population/macarthur-and-macarthur2-folder
	# Test renaming of layers
	./tippecanoe -q -f -Z5 -z10 -o tests/join-population/macarthur.mbtiles -l macarthur1 tests/join-population/macarthur.json
	./tippecanoe -q -f -Z5 -z10 -o tests/join-population/macarthur2.mbtiles -l macarthur2 tests/join-population/macarthur2.json
	./tile-join -q -R macarthur1:one --rename-layer=macarthur2:two -f -o tests/join-population/renamed.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	./tippecanoe-decode -x generator tests/join-population/renamed.mbtiles > tests/join-population/renamed.mbtiles.json.check
	cmp tests/join-population/renamed.mbtiles.json.check tests/join-population/renamed.mbtiles.json
	rm -f tests/join-population/renamed.mbtiles.json.check tests/join-population/renamed.mbtiles.json.check tests/join-population/macarthur.mbtiles tests/join-population/macarthur2.mbtiles
	# Make sure the concatenated name isn't too long
	./tippecanoe -q -f -z0 -n 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ' -o tests/join-population/macarthur.mbtiles tests/join-population/macarthur.json
	./tile-join -f -o tests/join-population/concat.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur.mbtiles tests/join-population/macarthur.mbtiles
	./tippecanoe-decode -x generator tests/join-population/concat.mbtiles > tests/join-population/concat.mbtiles.json.check
	cmp tests/join-population/concat.mbtiles.json.check tests/join-population/concat.mbtiles.json
	rm tests/join-population/concat.mbtiles.json.check tests/join-population/concat.mbtiles tests/join-population/macarthur.mbtiles

join-filter-test:
	# Comes out different from the direct tippecanoe run because null attributes are lost
	./tippecanoe -q -z0 -f -o tests/feature-filter/out/all.mbtiles tests/feature-filter/in.json
	./tile-join -q -J tests/feature-filter/filter -f -o tests/feature-filter/out/filtered.mbtiles tests/feature-filter/out/all.mbtiles
	./tippecanoe-decode -x generator tests/feature-filter/out/filtered.mbtiles > tests/feature-filter/out/filtered.json.check
	cmp tests/feature-filter/out/filtered.json.check tests/feature-filter/out/filtered.json.standard
	rm -f tests/feature-filter/out/filtered.json.check tests/feature-filter/out/filtered.mbtiles tests/feature-filter/out/all.mbtiles
	# Test zoom level filtering
	./tippecanoe -q -r1 -z8 -f -o tests/feature-filter/out/places.mbtiles tests/ne_110m_populated_places/in.json
	./tile-join -q -J tests/feature-filter/places-filter -f -o tests/feature-filter/out/places-filter.mbtiles tests/feature-filter/out/places.mbtiles
	./tippecanoe-decode -x generator tests/feature-filter/out/places-filter.mbtiles > tests/feature-filter/out/places-filter.mbtiles.json.check
	cmp tests/feature-filter/out/places-filter.mbtiles.json.check tests/feature-filter/out/places-filter.mbtiles.json.standard
	rm -f tests/feature-filter/out/places.mbtiles tests/feature-filter/out/places-filter.mbtiles tests/feature-filter/out/places-filter.mbtiles.json.check

json-tool-test: tippecanoe-json-tool
	./tippecanoe-json-tool -e GEOID10 tests/join-population/tabblock_06001420.json | sort > tests/join-population/tabblock_06001420.json.sort
	./tippecanoe-json-tool -c tests/join-population/population.csv tests/join-population/tabblock_06001420.json.sort > tests/join-population/tabblock_06001420.json.sort.joined
	./tippecanoe-json-tool --empty-csv-columns-are-null -c tests/join-population/population.csv tests/join-population/tabblock_06001420.json.sort > tests/join-population/tabblock_06001420-null.json.sort.joined
	cmp tests/join-population/tabblock_06001420.json.sort.joined tests/join-population/tabblock_06001420.json.sort.joined.standard
	cmp tests/join-population/tabblock_06001420-null.json.sort.joined tests/join-population/tabblock_06001420-null.json.sort.joined.standard
	rm -f tests/join-population/tabblock_06001420.json.sort tests/join-population/tabblock_06001420.json.sort.joined
	rm -f tests/join-population/tabblock_06001420-null.json.sort.joined

allow-existing-test:
	# Make a tileset
	./tippecanoe -q -Z0 -z0 -f -o tests/allow-existing/both.mbtiles tests/coalesce-tract/tl_2010_06001_tract10.json
	# Writing to existing should fail
	if ./tippecanoe -q -Z1 -z1 -o tests/allow-existing/both.mbtiles tests/coalesce-tract/tl_2010_06001_tract10.json; then exit 1; else exit 0; fi
	# Replace existing
	./tippecanoe -q -Z8 -z9 -f -o tests/allow-existing/both.mbtiles tests/coalesce-tract/tl_2010_06001_tract10.json
	./tippecanoe -q -Z10 -z11 -F -o tests/allow-existing/both.mbtiles tests/coalesce-tract/tl_2010_06001_tract10.json
	./tippecanoe-decode -x generator -x generator_options tests/allow-existing/both.mbtiles > tests/allow-existing/both.mbtiles.json.check
	cmp tests/allow-existing/both.mbtiles.json.check tests/allow-existing/both.mbtiles.json
	# Make a tileset
	./tippecanoe -q -Z0 -z0 -f -e tests/allow-existing/both.dir tests/coalesce-tract/tl_2010_06001_tract10.json
	# Writing to existing should fail
	if ./tippecanoe -q -Z1 -z1 -e tests/allow-existing/both.dir tests/coalesce-tract/tl_2010_06001_tract10.json; then exit 1; else exit 0; fi
	# Replace existing
	./tippecanoe -q -Z8 -z9 -f -e tests/allow-existing/both.dir tests/coalesce-tract/tl_2010_06001_tract10.json
	./tippecanoe -q -Z10 -z11 -F -e tests/allow-existing/both.dir tests/coalesce-tract/tl_2010_06001_tract10.json
	./tippecanoe-decode -x generator -x generator_options tests/allow-existing/both.dir | sed 's/both\.dir/both.mbtiles/g' > tests/allow-existing/both.dir.json.check
	cmp tests/allow-existing/both.dir.json.check tests/allow-existing/both.mbtiles.json
	rm -r tests/allow-existing/both.dir.json.check tests/allow-existing/both.dir tests/allow-existing/both.mbtiles.json.check tests/allow-existing/both.mbtiles

csv-test:
	# Reading from named CSV
	./tippecanoe -q -zg -f -o tests/csv/out.mbtiles tests/csv/ne_110m_populated_places_simple.csv
	./tippecanoe-decode -x generator -x generator_options tests/csv/out.mbtiles > tests/csv/out.mbtiles.json.check
	cmp tests/csv/out.mbtiles.json.check tests/csv/out.mbtiles.json
	rm -f tests/csv/out.mbtiles.json.check tests/csv/out.mbtiles
	# Reading from named CSV, with nulls
	./tippecanoe -q --empty-csv-columns-are-null -zg -f -o tests/csv/out-null.mbtiles tests/csv/ne_110m_populated_places_simple.csv
	./tippecanoe-decode -x generator tests/csv/out-null.mbtiles > tests/csv/out-null.mbtiles.json.check
	cmp tests/csv/out-null.mbtiles.json.check tests/csv/out-null.mbtiles.json
	rm -f tests/csv/out-null.mbtiles.json.check tests/csv/out-null.mbtiles
	# Same, but specifying csv with -L format
	./tippecanoe -q -zg -f -o tests/csv/out.mbtiles -L'{"file":"", "format":"csv", "layer":"ne_110m_populated_places_simple"}' < tests/csv/ne_110m_populated_places_simple.csv
	./tippecanoe-decode -x generator -x generator_options tests/csv/out.mbtiles > tests/csv/out.mbtiles.json.check
	cmp tests/csv/out.mbtiles.json.check tests/csv/out.mbtiles.json
	rm -f tests/csv/out.mbtiles.json.check tests/csv/out.mbtiles

layer-json-test:
	# GeoJSON with description and named layer
	./tippecanoe -q -z0 -r1 -yNAME -f -o tests/layer-json/out.mbtiles -L'{"file":"tests/ne_110m_populated_places/in.json", "description":"World cities", "layer":"places"}'
	./tippecanoe-decode -x generator -x generator_options tests/layer-json/out.mbtiles > tests/layer-json/out.mbtiles.json.check
	cmp tests/layer-json/out.mbtiles.json.check tests/layer-json/out.mbtiles.json
	rm -f tests/layer-json/out.mbtiles.json.check tests/layer-json/out.mbtiles
	# Same, but reading from the standard input
	./tippecanoe -q -z0 -r1 -yNAME -f -o tests/layer-json/out.mbtiles -L'{"file":"", "description":"World cities", "layer":"places"}' < tests/ne_110m_populated_places/in.json
	./tippecanoe-decode -x generator -x generator_options tests/layer-json/out.mbtiles > tests/layer-json/out.mbtiles.json.check
	cmp tests/layer-json/out.mbtiles.json.check tests/layer-json/out.mbtiles.json
	rm -f tests/layer-json/out.mbtiles.json.check tests/layer-json/out.mbtiles

# Use this target to regenerate the standards that the tests are compared against
# after making a change that legitimately changes their output

prep-test: $(TESTS)

tests/%.json: Makefile tippecanoe tippecanoe-decode
	./tippecanoe -q -a@ -f -o $@.check.mbtiles $(subst @,:,$(subst %,/,$(subst _, ,$(patsubst %.json,%,$(word 4,$(subst /, ,$@)))))) $(foreach suffix,$(suffixes),$(sort $(wildcard $(subst $(SPACE),/,$(wordlist 1,2,$(subst /, ,$@)))/*.$(suffix))))
	./tippecanoe-decode -x generator $@.check.mbtiles > $@
	cmp $(patsubst %.check,%,$@) $@
	rm $@.check.mbtiles
