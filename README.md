tippecanoe
==========

Builds [vector tilesets](https://www.mapbox.com/developers/vector-tiles/) from large collections of [GeoJSON](http://geojson.org/)
features. This is a tool for [making maps from huge datasets](MADE_WITH.md).

Intent
------

The goal of Tippecanoe is to enable making a scale-independent view of your data,
so that at any level from the entire world to a single building, you can see
the density and texture of the data rather than a simplification from dropping
supposedly unimportant features or clustering or aggregating them.

If you give it all of OpenStreetMap and zoom out, it should give you back
something that looks like "[All Streets](http://benfry.com/allstreets/map5.html)"
rather than something that looks like an Interstate road atlas.

If you give it all the building footprints in Los Angeles and zoom out
far enough that most individual buildings are no longer discernable, you
should still be able to see the extent and variety of development in every neighborhood,
not just the largest downtown buildings.

If you give it a collection of years of tweet locations, you should be able to
see the shape and relative popularity of every point of interest and every
significant travel corridor.

Installation
------------

The easiest way to install tippecanoe on OSX is with [Homebrew](http://brew.sh/):

```js
$ brew install tippecanoe
```

Usage
-----

```sh
$ tippecanoe -o file.mbtiles [file.json ...]
```

If no files are specified, it reads GeoJSON from the standard input.
If multiple files are specified, each is placed in its own layer.

The GeoJSON features need not be wrapped in a FeatureCollection.
You can concatenate multiple GeoJSON features or files together,
and it will parse out the features and ignore whatever other objects
it encounters.

Options
-------

### Naming

 * -l _name_: Layer name (default "file" if source is file.json or output is file.mbtiles). Only works if there is only one layer.
 * -n _name_: Human-readable name (default file.json)

### File control

 * -o _file_.mbtiles: Name the output file.
 * -f: Delete the mbtiles file if it already exists instead of giving an error
 * -t _directory_: Put the temporary files in _directory_.

### Zoom levels and resolution

 * -z _zoom_: Base (maxzoom) zoom level (default 14)
 * -Z _zoom_: Lowest (minzoom) zoom level (default 0)
 * -d _detail_: Detail at base zoom level (default 12 at -z14 or higher, or 13 at -z13 or lower. Detail beyond 13 has rendering problems with Mapbox GL.)
 * -D _detail_: Detail at lower zoom levels (default 10, for tile resolution of 1024)
 * -m _detail_: Minimum detail that it will try if tiles are too big at regular detail (default 7)
 * -b _pixels_: Buffer size where features are duplicated from adjacent tiles. Units are "screen pixels"--1/256th of the tile width or height. (default 5)

### Properties

 * -x _name_: Exclude the named properties from all features
 * -y _name_: Include the named properties in all features, excluding all those not explicitly named
 * -X: Exclude all properties and encode only geometries

### Point simplification

 * -r _rate_: Rate at which dots are dropped at lower zoom levels (default 2.5)
 * -g _gamma_: Rate at which especially dense dots are dropped (default 0, for no effect). A gamma of 2 reduces the number of dots less than a pixel apart to the square root of their original number.

### Doing more

 * -ac: Coalesce adjacent line and polygon features that have the same properties
 * -ar: Try reversing the directions of lines to make them coalesce and compress better
 * -ao: Reorder features to put ones with the same properties in sequence, to try to get them to coalesce
 * -al: Let "dot" dropping at lower zooms apply to lines too

### Doing less

 * -ps: Don't simplify lines
 * -pf: Don't limit tiles to 200,000 features
 * -pk: Don't limit tiles to 500K bytes
 * -pd: Dynamically drop some fraction of features from large tiles to keep them under the 500K size limit. It will probably look ugly at the tile boundaries.
 * -pi: Preserve the original input order of features as the drawing order instead of ordering geographically. (This is implemented as a restoration of the original order at the end, so that dot-dropping is still geographic, which means it also undoes -ao).
 * -q: Work quietly instead of reporting progress

Example
-------

```sh
$ tippecanoe -o alameda.mbtiles -l alameda -n "Alameda County from TIGER" -z13 tl_2014_06001_roads.json
```

```
$ cat tiger/tl_2014_*_roads.json | tippecanoe -o tiger.mbtiles -l roads -n "All TIGER roads, one zoom" -z12 -Z12 -d14 -x LINEARID -x RTTYP
```

GeoJSON extension
-----------------

Tippecanoe defines a GeoJSON extension that you can use to specify the minimum and/or maximum zoom level
at which an individual feature will be included in the vector tile dataset being produced.
If you have a feature like this:

```
{
    "type" : "Feature",
    "tippecanoe" : { "maxzoom" : 9, "minzoom" : 4 },
    "properties" : { "FULLNAME" : "N Vasco Rd", },
    "geometry" : {
        "type" : "LineString",
        "coordinates" : [ [ -121.733350, 37.767671 ], [ -121.733600, 37.767483 ], [ -121.733131, 37.766952 ] ]
    }
}
```

with a `tippecanoe` object specifiying a `maxzoom` of 9 and a `minzoom` of 4, the feature
will only appear in the vector tiles for zoom levels 4 through 9. Note that the `tippecanoe`
object belongs to the Feature, not to its `properties`.

Point styling
-------------

To provide a consistent density gradient as you zoom, the Mapbox Studio style needs to be
coordinated with the base zoom level and dot-dropping rate. You can use this shell script to
calculate the appropriate marker-width at high zoom levels to match the fraction of dots
that were dropped at low zoom levels.

If you used `-z` to change the base zoom level or `-r` to change the
dot-dropping rate, replace them in the `basezoom` and `rate` below.

    awk 'BEGIN {
        dotsize = 2;    # up to you to decide
        basezoom = 14;  # tippecanoe -z 14
        rate = 2.5;     # tippecanoe -r 2.5

        print "  marker-line-width: 0;";
        print "  marker-ignore-placement: true;";
        print "  marker-allow-overlap: true;";
        print "  marker-width: " dotsize ";";
        for (i = basezoom + 1; i <= 22; i++) {
            print "  [zoom >= " i "] { marker-width: " (dotsize * exp(log(sqrt(rate)) * (i - basezoom))) "; }";
        }

        exit(0);
    }'

Geometric simplifications
-------------------------

At every zoom level, line and polygon features are subjected to Douglas-Peucker
simplification to the resolution of the tile.

For point features, it drops 1/2.5 of the dots for each zoom level above the base.
I don't know why 2.5 is the appropriate number, but the densities of many different
data sets fall off at about this same rate. You can use -r to specify a different rate.

You can use the gamma option to thin out especially dense clusters of points.
For any area where dots are closer than one pixel together (at whatever zoom level),
a gamma of 3, for example, will reduce these clusters to the cube root of their original density.

For line features, it drops any features that are too small to draw at all.
This still leaves the lower zooms too dark (and too dense for the 500K tile limit,
in some places), so I need to figure out an equitable way to throw features away.

Any polygons that are smaller than a minimum area (currently 9 square subpixels) will
have their probability diffused, so that some of them will be drawn as a square of
this minimum size and others will not be drawn at all, preserving the total area that
all of them should have had together.

Features in the same tile that share the same type and attributes are coalesced
together into a single geometry. You are strongly encouraged to use -x to exclude
any unnecessary properties to reduce wasted file size.

If a tile is larger than 500K, it will try encoding that tile at progressively
lower resolutions before failing if it still doesn't fit.

Development
-----------

Requires protoc and sqlite3. Rebuilding the manpage
uses md2man (`gem install md2man`).

MacOS:

    brew install protobuf

Linux:

    sudo apt-get install libprotobuf-dev
    sudo apt-get install protobuf-compiler
    sudo apt-get install libsqlite3-dev

Then build:

    make

and perhaps

    make install

Examples
------

Check out [some examples of maps made with tippecanoe](MADE_WITH.md)

Name
----

The name is [a joking reference](http://en.wikipedia.org/wiki/Tippecanoe_and_Tyler_Too) to a "tiler" for making map tiles.

tile-join
=========

Tile-join is a tool for joining new attributes from a CSV file to features that
have already been tiled with tippecanoe. It reads the tiles from an existing .mbtiles
file, matches them against the records of the CSV, and writes out a new tileset.

The options are:

 * -o *out.mbtiles*: Write the new tiles to the specified .mbtiles file
 * -f: Remove *out.mbtiles* if it already exists
 * -c *match.csv*: Use *match.csv* as the source for new attributes to join to the features. The first line of the file should be the key names; the other lines are values. The first column is the one to match against the existing features; the other columns are the new data to add.
 * -x *key*: Remove attributes of type *key* from the output. You can use this to remove the field you are matching against if you no longer need it after joining, or to remove any other attributes you don't want.
 * -i: Only include features that matched the CSV.

Because tile-join just copies the geometries to the new .mbtiles without processing them,
it doesn't have any of tippecanoe's recourses if the new tiles are bigger than the 500K tile limit.
If a tile is too big, it is just left out of the new tileset.

Example
-------

Imagine you have a tileset of census blocks:

```sh
curl -O http://www2.census.gov/geo/tiger/TIGER2010/TABBLOCK/2010/tl_2010_06001_tabblock10.zip
unzip tl_2010_06001_tabblock10.zip
ogr2ogr -f GeoJSON tl_2010_06001_tabblock10.json tl_2010_06001_tabblock10.shp
./tippecanoe -o tl_2010_06001_tabblock10.mbtiles tl_2010_06001_tabblock10.json
```

and a CSV of their populations:

```sh
curl -O http://www2.census.gov/census_2010/01-Redistricting_File--PL_94-171/California/ca2010.pl.zip
unzip -p ca2010.pl.zip cageo2010.pl |
awk 'BEGIN {
    print "GEOID10,population"
}
(substr($0, 9, 3) == "750") {
    print "\"" substr($0, 28, 2) substr($0, 30, 3) substr($0, 55, 6) substr($0, 62, 4) "\"," (0 + substr($0, 328, 9))
}' > population.csv
```

which looks like this:

```
GEOID10,population
"060014277003018",0
"060014283014046",0
"060014284001020",0
...
"060014507501001",202
"060014507501002",119
"060014507501003",193
"060014507501004",85
...
```

Then you can join those populations to the geometries and discard the no-longer-needed ID field:

```sh
./tile-join -o population.mbtiles -x GEOID10 -c population.csv tl_2010_06001_tabblock10.mbtiles
```
