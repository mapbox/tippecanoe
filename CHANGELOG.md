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
