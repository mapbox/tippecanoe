## 1.27.12

* Check integrity of sqlite3 file before decoding or tile-joining

## 1.27.11

* Always include tile and layer in tippecanoe-decode, fixing corrupt JSON.
* Clean up writing of JSON in general.

## 1.27.10

* Add --progress-interval setting to reduce progress indicator frequency

## 1.27.9

* Make clusters look better by averaging locations of clustered points

## 1.27.8

* Add --accumulate-attribute to keep attributes of dropped, coalesced,
  or clustered features
* Make sure numeric command line arguments are actually numbers
* Don't coalesce features whose non-string-pool attributes don't match

## 1.27.7

* Add an option to produce only a single tile
* Retain non-ASCII characters in layernames generated from filenames
* Remember to close input files after reading them
* Add --coalesce-fraction-as-needed and --coalesce-densest-as-needed
* Report distances in both feet and meters

## 1.27.6

* Fix opportunities for integer overflow and out-of-bounds references

## 1.27.5

* Add --cluster-densest-as-needed to cluster features
* Add --maximum-tile-features to set the maximum number of features in a tile

## 1.27.4

* Support CSV point input
* Don't coalesce features that have different IDs but are otherwise identical
* Remove the 700-point limit on coalesced features, since polygon merging
  is no longer a performance problem

## 1.27.3

* Clean up duplicated code for reading tiles from a directory

## 1.27.2

* Tippecanoe-decode can decode directories of tiles, not just mbtiles
* The --allow-existing option works on directories of tiles
* Trim .geojson, not just .json, when making layer names from filenames

## 1.27.1

* Fix a potential null pointer when parsing GeoJSON with bare geometries
* Fix a bug that could cause the wrong features to be coalesced when
  input was parsed in parallel

## 1.27.0

* Add tippecanoe-json-tool for sorting and joining GeoJSON files
* Fix problem where --detect-shared-borders could simplify polygons away
* Attach --coalesce-smallest-as-needed leftovers to the last feature, not the first
* Fix overflow when iterating through 0-length lists backwards

## 1.26.7

* Add an option to quiet the progress indicator but not warnings
* Enable more compiler warnings and fix related problems

## 1.26.6

* Be more careful about checking for overflow when parsing numbers

## 1.26.5

* Support UTF-16 surrogate pairs in JSON strings
* Support arbitrarily long lines in CSV files.
* Treat CSV fields as numbers only if they follow JSON number syntax

## 1.26.4

* Array bounds bug fix in binary to decimal conversion library

## 1.26.3

* Guard against impossible coordinates when decoding tilesets

## 1.26.2

* Make sure to encode tile-joined integers as ints, not doubles

## 1.26.1

* Add tile-join option to rename layers

## 1.26.0

Fix error when parsing attributes with empty-string keys

## 1.25.0

* Add --coalesce-smallest-as-needed strategy for reducing tile sizes
* Add --stats option to tipppecanoe-decode

## 1.24.1

* Limit the size and depth of the string pool for better performance

## 1.24.0

* Add feature filters using the Mapbox GL Style Specification filter syntax

## 1.23.0

* Add input support for Geobuf file format

## 1.22.2

* Add better diagnostics for NaN or Infinity in input JSON

## 1.22.1

* Fix tilestats generation when long string attribute values are elided
* Add option not to produce tilestats
* Add tile-join options to select zoom levels to copy

## 1.22.0

* Add options to filter each tile's contents through a shell pipeline

## 1.21.0

* Generate layer, feature, and attribute statistics as part of tileset metadata

## 1.20.1

* Close mbtiles file properly when there are no valid features in the input

## 1.20.0

* Add long options to tippecanoe-decode and tile-join. Add --quiet to tile-join.

## 1.19.3

* Upgrade protozero to version 1.5.2

## 1.19.2

* Ignore UTF-8 byte order mark if present

## 1.19.1

* Add an option to increase maxzoom if features are still being dropped

## 1.19.0

* Tile-join can merge and create directories, not only mbtiles
* Maxzoom guessing (-zg) takes into account resolution within each feature

## 1.18.2

* Fix crash with very long (>128K) attribute values

## 1.18.1

* Only warn once about invalid polygons in tippecanoe-decode

## 1.18.0

* Fix compression of tiles in tile-join
* Calculate the tileset bounding box in tile-join from the tile boundaries

## 1.17.7

* Enforce polygon winding and closure rules in tippecanoe-decode

## 1.17.6

* Add tile-join options to set name, attribution, description

## 1.17.5

* Preserve the tileset names from the source mbtiles in tile-join

## 1.17.4

* Fix RFC 8142 support: Don't try to split *all* memory mapped files

## 1.17.3

* Support RFC 8142 GeoJSON text sequences

## 1.17.2

* Organize usage output the same way as in the README

## 1.17.1

* Add -T option to coerce the types of feature attributes

## 1.17.0

* Add -zg option to guess an appropriate maxzoom

## 1.16.17

* Clean up JSON parsing at the end of each FeatureCollection
  to avoid running out of memory

## 1.16.16

* Add tile-join options to include or exclude specific layers

## 1.16.15

* Add --output-to-directory and --no-tile-compression options

## 1.16.14

* Add --description option for mbtiles metadata
* Clean up some utility functions

## 1.16.13

* Add --detect-longitude-wraparound option

## 1.16.12

* Stop processing higher zooms when a feature reaches its explicit maxzoom tag

## 1.16.11

* Remove polygon splitting, since polygon cleaning is now fast enough

## 1.16.10

* Add a tippecanoe-decode option to specify layer names

## 1.16.9

* Clean up layer name handling to fix layer merging crash

## 1.16.8

* Fix some code that could sometimes try to divide by zero
* Add check for $TIPPECANOE_MAX_THREADS environmental variable

## 1.16.7

* Fix area of placeholders for degenerate multipolygons

## 1.16.6

* Upgrade Wagyu to 0.3.0; downgrade C++ requirement to C++ 11

## 1.16.5

* Add -z and -Z options to tippecanoe-decode

## 1.16.4

* Use Wagyu's quick_lr_clip() instead of a separate implementation

## 1.16.3

* Upgrade Wagyu to bfbf2893

## 1.16.2

* Associate attributes with the right layer when explicitly tagged

## 1.16.1

* Choose a deeper starting tile than 0/0/0 if there is one that contains
  all the features

## 1.16.0

* Switch from Clipper to Wagyu for polygon topology correction

## 1.15.4

* Dot-dropping with -r/-B doesn't apply if there is a per-feature minzoom tag

## 1.15.3

* Round coordinates in low-zoom grid math instead of truncating

## 1.15.2

* Add --grid-low-zooms option to snap low-zoom features to the tile grid

## 1.15.1

* Stop --drop-smallest-as-needed from always dropping all points

## 1.15.0

* New strategies for making tiles smaller, with uniform behavior across
  the whole zoom level: --increase-gamma-as-needed,
  --drop-densest-as-needed, --drop-fraction-as-needed,
  --drop-smallest-as-needed.
* Option to specify the maximum tile size in bytes
* Option to turn off tiny polygon reduction
* Better error checking in JSON parsing

## 1.14.4

* Make -B/-r feature-dropping consistent between tiles and zoom levels

## 1.14.3

* Add --detect-shared-borders option for better polygon simplification

## 1.14.2

* Enforce that string feature attributes must be encoded as UTF-8

## 1.14.1

* Whitespace after commas in tile-join .csv input is no longer significant

## 1.14.0

* Tile-join is multithreaded and can merge multiple vector mbtiles files together

## 1.13.0

* Add the ability to specify layer names within the GeoJSON input

## 1.12.11

* Don't try to revive a placeholder for a degenerate polygon that had negative area

## 1.12.10

* Pass feature IDs through in tile-join

## 1.12.9

* Clean up parsing and serialization. Provide some context with parsing errors.

## 1.12.8

* Fix the spelling of the --preserve-input-order option

## 1.12.7

* Support the "id" field of GeoJSON objects and vector tile features

## 1.12.6

* Fix error reports when reading from an empty file with parallel input

## 1.12.5

* Add an option to vary the level of line and polygon simplification
* Be careful not to produce an empty tile if there was a feature with
  empty geometry.

## 1.12.4

* Be even more careful not to produce features with empty geometry

## 1.12.3

* Fix double-counted progress in the progress indicator

## 1.12.2

* Add ability to specify a projection to tippecanoe-decode

## 1.12.1

* Fix incorrect tile layer version numbers in tile-join output

## 1.12.0

* Fix a tile-join bug that would retain fields that were supposed to be excluded

## 1.11.9

* Add minimal support for alternate input projections (EPSG:3857).

## 1.11.8

* Add an option to calculate the density of features as a feature attribute

## 1.11.7

* Keep metadata together with geometry for features that don't span many tiles,
  to avoid extra memory load from indexing into a separate metadata file

## 1.11.6

* Reduce the size of critical data structures to reduce dynamic memory use

## 1.11.5

* Let zoom level 0 have just as much extent and buffer as any other zoom
* Fix tippecanoe-decode bug that would sometimes show outer rings as inner

## 1.11.4

* Don't let polygons with nonzero area disappear during cleaning

## 1.11.3

* Internal code cleanup

## 1.11.2

* Update Clipper to fix potential crash

## 1.11.1

* Make better use of C++ standard libraries

## 1.11.0

* Convert C source files to C++

## 1.10.0

* Upgrade Clipper to fix potential crashes and improve polygon topology

## 1.9.16

* Switch to protozero as the library for reading and writing protocol buffers

## 1.9.15

* Add option not to clip features

## 1.9.14

* Clean up polygons after coalescing, if necessary

## 1.9.13

* Don't trust the OS so much about how many files can be open

## 1.9.12

* Limit the size of the parallel parsing streaming input buffer
* Add an option to set the tileset's attribution

## 1.9.11

* Fix a line simplification crash when a segment degenerates to a single point

## 1.9.10

* Warn if temporary disk space starts to run low

## 1.9.9

* Add --drop-polygons to drop a fraction of polygons by zoom level
* Only complain once about failing to clean polygons

## 1.9.8

* Use an on-disk radix sort for the index to control virtual memory thrashing
  when the geometry and index are too large to fit in memory

## 1.9.7

* Fix build problem (wrong spelling of long long max/min constants)

## 1.9.6

* Add an option to give specific layer names to specific input files

## 1.9.5

* Remove temporary files that were accidentally left behind
* Be more careful about checking memory allocations and array bounds
* Add GNU-style long options

## 1.9.4

* Tippecanoe-decode can decode .pbf files that aren't in an .mbtiles container

## 1.9.3

* Don't get stuck in a loop trying to split up very small, very complicated polygons

## 1.9.2

* Increase maximum tile size for tippecanoe-decode

## 1.9.1

* Incorporate Mapnik's Clipper upgrades for consistent results between Mac and Linux

## 1.9.0

* Claim vector tile version 2 in mbtiles
* Split too-complex polygons into multiple features

## 1.8.1

* Bug fixes to maxzoom, and more tests

## 1.8.0

* There are tests that can be run with "make test".

## 1.7.2

* Feature properties that are arrays or hashes get stringified
  rather than being left out with a warning.

## 1.7.1

* Make clipping behavior with no buffer consistent with Mapnik.
  Features that are exactly on a tile boundary appear in both tiles.

## 1.7.0

* Parallel processing of input with -P works with streamed input too
* Error handling if unsupported options given to -p or -a

## 1.6.4

* Fix crashing bug when layers are being merged with -l

## 1.6.3

* Add an option to do line simplification only at zooms below maxzoom

## 1.6.2

* Make sure line simplification matches on opposite sides of a tile boundary

## 1.6.1

* Use multiple threads for line simplification and polygon cleaning

## 1.6.0

* Add option of parallelized input when reading from a line-delimited file

## 1.5.1

* Fix internal error when number of CPUs is not a power of 2
* Add missing #include

## 1.5.0

* Base zoom for dot-dropping can be specified independently of
  maxzoom for tiling.
* Tippecanoe can calculate a base zoom and drop rate for you.

## 1.4.3

* Encode numeric attributes as integers instead of floating point if possible

## 1.4.2

* Bug fix for problem that would occasionally produce empty point geometries
* More bug fixes for polygon generation

## 1.4.1

* Features that cross the antimeridian are split into two parts instead
  of being partially lost off the edge

## 1.4.0

* More polygon correctness
* Query the system for the number of available CPUs instead of guessing
* Merge input files into one layer if a layer name is specified
* Document and install tippecanoe-enumerate and tippecanoe-decode

## 1.3.0

* Tile generation is multithreaded to take advantage of multiple CPUs
* More compact data representation reduces memory usage and improves speed
* Polygon clipping uses [Clipper](http://www.angusj.com/delphi/clipper/documentation/Docs/_Body.htm)
  and makes sure interior and exterior rings are distinguished by winding order
* Individual GeoJSON features can specify their own minzoom and maxzoom
* New `tile-join` utility can add new properties from a CSV file to an existing tileset
* Feature coalescing, line-reversing, and reordering by attribute are now options, not defaults
* Output of `decode` utility is now in GeoJSON format
* Tile generation with a minzoom spends less time on unused lower zoom levels
* Bare geometries without a Feature wrapper are accepted
* Default tile resolution is 4096 units at all zooms since renderers assume it

## 1.2.0

* Switched to top-down rendering, yielding performance improvements
* Add a dot-density gamma feature to thin out especially dense clusters
* Add support for multiple layers, making it possible to include more
  than one GeoJSON featurecollection in a map. [#29](https://github.com/mapbox/tippecanoe/pull/29)
* Added flags that let you optionally avoid simplifying lines, restricting
  maximum tile sizes, and coalescing features [#30](https://github.com/mapbox/tippecanoe/pull/30)
* Added check that minimum zoom level is less than maximum zoom level
* Added `-v` flag to check tippecanoe's version
