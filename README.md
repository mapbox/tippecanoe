tippecanoe
==========

Builds [vector tilesets](https://www.mapbox.com/developers/vector-tiles/) from large (or small) collections of [GeoJSON](http://geojson.org/), [Geobuf](https://github.com/mapbox/geobuf), or [Shapefile](https://en.wikipedia.org/wiki/Shapefile) features,
[like these](MADE_WITH.md).

[![Build Status](https://travis-ci.org/mapbox/tippecanoe.svg)](https://travis-ci.org/mapbox/tippecanoe)
[![Coverage Status](https://coveralls.io/repos/mapbox/tippecanoe/badge.svg?branch=master&service=github)](https://coveralls.io/github/mapbox/tippecanoe?branch=master)

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
$ tippecanoe -o file.mbtiles [options] [file.json file.geobuf ...]
```

If no files are specified, it reads GeoJSON from the standard input.
If multiple files are specified, each is placed in its own layer.

The GeoJSON features need not be wrapped in a FeatureCollection.
You can concatenate multiple GeoJSON features or files together,
and it will parse out the features and ignore whatever other objects
it encounters.

Try this first
--------------

If you aren't sure what options to use, try this:

```sh
$ tippecanoe -o out.mbtiles -zg --drop-densest-as-needed in.geojson
```

The `-zg` option will make Tippecanoe choose a maximum zoom level that should be
high enough to reflect the precision of the original data. (If it turns out still
not to be as detailed as you want, use `-z` manually with a higher number.)

If the tiles come out too big, the `--drop-densest-as-needed` option will make
Tippecanoe try dropping what should be the least visible features at each zoom level.
(If it drops too many features, use `-x` to leave out some feature attributes that
you didn't really need.)

Examples
--------

Create a tileset of TIGER roads for Alameda County, to zoom level 13, with a custom layer name and description:

```sh
$ tippecanoe -o alameda.mbtiles -l alameda -n "Alameda County from TIGER" -z13 tl_2014_06001_roads.json
```

Create a tileset of all TIGER roads, at only zoom level 12, but with higher detail than normal,
with a custom layer name and description, and leaving out the `LINEARID` and `RTTYP` attributes:

```
$ cat tiger/tl_2014_*_roads.json | tippecanoe -o tiger.mbtiles -l roads -n "All TIGER roads, one zoom" -z12 -Z12 -d14 -x LINEARID -x RTTYP
```

Options
-------

There are a lot of options. A lot of the time you won't want to use any of them
other than `-o` _output_`.mbtiles` to name the output file, and probably `-f` to
delete the file that already exists with that name.

If you aren't sure what the right maxzoom is for your data, `-zg` will guess one for you
based on the density of features.

Tippecanoe will normally drop a fraction of point features at zooms below the maxzoom,
to keep the low-zoom tiles from getting too big. If you have a smaller data set where
all the points would fit without dropping any of them, use `-r1` to keep them all.
If you do want point dropping, but you still want the tiles to be denser than `-zg`
thinks they should be, use `-B` to set a basezoom lower than the maxzoom.

If some of your tiles are coming out too big in spite of the settings above, you will
often want to use `--drop-densest-as-needed` to drop whatever fraction of the features
is necessary at each zoom level to make that zoom level's tiles work.

If your features have a lot of attributes, use `-y` to keep only the ones you really need.

If your input is formatted as newline-delimited GeoJSON, use `-P` to make input parsing a lot faster.

### Output tileset

 * `-o` _file_`.mbtiles` or `--output=`_file_`.mbtiles`: Name the output file.
 * `-e` _directory_ or `--output-to-directory`=_directory_: Write tiles to the specified *directory* instead of to an mbtiles file.
 * `-f` or `--force`: Delete the mbtiles file if it already exists instead of giving an error
 * `-F` or `--allow-existing`: Proceed (without deleting existing data) if the metadata or tiles table already exists
   or if metadata fields can't be set. You probably don't want to use this.

### Tileset description and attribution

 * `-n` _name_ or `--name=`_name_: Human-readable name for the tileset (default file.json)
 * `-A` _text_ or `--attribution=`_text_: Attribution (HTML) to be shown with maps that use data from this tileset.
 * `-N` _description_ or `--description=`_description_: Description for the tileset (default file.mbtiles)

### Input files and layer names

 * _name_`.json` or _name_`.geojson`: Read the named GeoJSON input file into a layer called _name_.
 * _name_`.geobuf` or _name_`.geobuf`: Read the named Geobuf input file into a layer called _name_.
 * _name_`.shp` or _name_`.shp`: Read the named Shapefile input file into a layer called _name_. Note that _name_`.dbf` must also exist.
 * `-l` _name_ or `--layer=`_name_: Use the specified layer name instead of deriving a name from the input filename or output tileset. If there are multiple input files
   specified, the files are all merged into the single named layer, even if they try to specify individual names with `-L`.
 * `-L` _name_`:`_file.json_ or `--named-layer=`_name_`:`_file.json_: Specify layer names for individual files. If your shell supports it, you can use a subshell redirect like `-L` _name_`:<(cat dir/*.json)` to specify a layer name for the output of streamed input.

### Parallel processing of input

 * `-P` or `--read-parallel`: Use multiple threads to read different parts of each GeoJSON input file at once.
   This will only work if the input is line-delimited JSON with each Feature on its
   own line, because it knows nothing of the top-level structure around the Features. Spurious "EOF" error
   messages may result otherwise.
   Performance will be better if the input is a named file that can be mapped into memory
   rather than a stream that can only be read sequentially.

If the input file begins with the [RFC 8142](https://tools.ietf.org/html/rfc8142) record separator,
parallel processing of input will be invoked automatically, splitting at record separators rather
than at all newlines.

Parallel processing will also be automatic if the input file is in Geobuf format.

### Projection of input

 * `-s` _projection_ or `--projection=`_projection_: Specify the projection of the input data. Currently supported are `EPSG:4326` (WGS84, the default) and `EPSG:3857` (Web Mercator). In general you should use WGS84 for your input files if at all possible.

### Zoom levels

 * `-z` _zoom_ or `--maximum-zoom=`_zoom_: Maxzoom: the highest zoom level for which tiles are generated (default 14)
 * `-zg` or `--maximum-zoom=g`: Guess what is probably a reasonable maxzoom based on the spacing of features.
 * `-Z` _zoom_ or `--minimum-zoom=`_zoom_: Minzoom: the lowest zoom level for which tiles are generated (default 0)
 * `-ae` or `--extend-zooms-if-still-dropping`: Increase the maxzoom if features are still being dropped at that zoom level.
   The detail and simplification options that ordinarily apply only to the maximum zoom level will apply both to the originally
   specified maximum zoom and to any levels added beyond that.

### Tile resolution

 * `-d` _detail_ or `--full-detail=`_detail_: Detail at max zoom level (default 12, for tile resolution of 2^12=4096)
 * `-D` _detail_ or `--low-detail=`_detail_: Detail at lower zoom levels (default 12, for tile resolution of 2^12=4096)
 * `-m` _detail_ or `--minimum-detail=`_detail_: Minimum detail that it will try if tiles are too big at regular detail (default 7)

All internal math is done in terms of a 32-bit tile coordinate system, so 1/(2^32) of the size of Earth,
or about 1cm, is the smallest distinguishable distance. If _maxzoom_  _detail_ > 32, no additional
resolution is obtained than by using a smaller _maxzoom_ or _detail_.

### Filtering feature attributes

 * `-x` _name_ or `--exclude=`_name_: Exclude the named properties from all features
 * `-y` _name_ or `--include=`_name_: Include the named properties in all features, excluding all those not explicitly named
 * `-X` or `--exclude-all`: Exclude all properties and encode only geometries
 * `-T`_attribute_`:`_type_ or `--attribute-type=`_attribute_`:`_type_: Coerce the named feature _attribute_ to be of the specified _type_.
   The _type_ may be `string`, `float`, `int`, or `bool`.
   If the type is `bool`, then original attributes of `0` (or, if numeric, `0.0`, etc.), `false`, `null`, or the empty string become `false`, and otherwise become `true`.
   If the type is `float` or `int` and the original attribute was non-numeric, it becomes `0`.
   If the type is `int` and the original attribute was floating-point, it is rounded to the nearest integer.
 * `-j` *filter* or `--feature-filter`=*filter*: Check features against a per-layer filter (as defined in the [Mapbox GL Style Specification](https://www.mapbox.com/mapbox-gl-js/style-spec/#types-filter)) and only include those that match. Any features in layers that have no filter specified will be passed through. Filters for the layer `"*"` apply to all layers.
 * `-J` *filter-file* or `--feature-filter-file`=*filter-file*: Like `-j`, but read the filter from a file.

Example: to find the Natural Earth countries with low `scalerank` but high `LABELRANK`:

```
tippecanoe -z5 -o filtered.mbtiles -j '{ "ne_10m_admin_0_countries": [ "all", [ "<", "scalerank", 3 ], [ ">", "LABELRANK", 5 ] ] }' ne_10m_admin_0_countries.geojson
```

### Dropping a fixed fraction of features by zoom level

 * `-r` _rate_ or `--drop-rate=`_rate_: Rate at which dots are dropped at zoom levels below basezoom (default 2.5).
   If you use `-rg`, it will guess a drop rate that will keep at most 50,000 features in the densest tile.
   You can also specify a marker-width with `-rg`*width* to allow fewer features in the densest tile to
   compensate for the larger marker, or `-rf`*number* to allow at most *number* features in the densest tile.
 * `-B` _zoom_ or `--base-zoom=`_zoom_: Base zoom, the level at and above which all points are included in the tiles (default maxzoom).
   If you use `-Bg`, it will guess a zoom level that will keep at most 50,000 features in the densest tile.
   You can also specify a marker-width with `-Bg`*width* to allow fewer features in the densest tile to
   compensate for the larger marker, or `-Bf`*number* to allow at most *number* features in the densest tile.
 * `-al` or `--drop-lines`: Let "dot" dropping at lower zooms apply to lines too
 * `-ap` or `--drop-polygons`: Let "dot" dropping at lower zooms apply to polygons too

### Dropping a fraction of features to keep under tile size limits

 * `-as` or `--drop-densest-as-needed`: If a tile is too large, try to reduce it to under 500K by increasing the minimum spacing between features. The discovered spacing applies to the entire zoom level.
 * `-ad` or `--drop-fraction-as-needed`: Dynamically drop some fraction of features from each zoom level to keep large tiles under the 500K size limit. (This is like `-pd` but applies to the entire zoom level, not to each tile.)
 * `-an` or `--drop-smallest-as-needed`: Dynamically drop the smallest features (physically smallest: the shortest lines or the smallest polygons) from each zoom level to keep large tiles under the 500K size limit. This option will not work for point features.
 * `-aN` or `--coalesce-smallest-as-needed`: Dynamically combine the smallest features (physically smallest: the shortest lines or the smallest polygons) from each zoom level into other nearby features to keep large tiles under the 500K size limit. This option will not work for point features, and will probably not help very much with LineStrings. It is mostly intended for polygons, to maintain the full original area covered by polygons while still reducing the feature count somehow. The attributes of the small polygons are *not* preserved into the combined features, only their geometry.
 * `-pd` or `--force-feature-limit`: Dynamically drop some fraction of features from large tiles to keep them under the 500K size limit. It will probably look ugly at the tile boundaries. (This is like `-ad` but applies to each tile individually, not to the entire zoom level.) You probably don't want to use this.

### Dropping tightly overlapping features

 * `-g` _gamma_ or `--gamma=_gamma`_: Rate at which especially dense dots are dropped (default 0, for no effect). A gamma of 2 reduces the number of dots less than a pixel apart to the square root of their original number.
 * `-aG` or `--increase-gamma-as-needed`: If a tile is too large, try to reduce it to under 500K by increasing the `-g` gamma. The discovered gamma applies to the entire zoom level. You probably want to use `--drop-densest-as-needed` instead.

### Line and polygon simplification

 * `-S` _scale_ or `--simplification=`_scale_: Multiply the tolerance for line and polygon simplification by _scale_. The standard tolerance tries to keep
   the line or polygon within one tile unit of its proper location. You can probably go up to about 10 without too much visible difference.
 * `-ps` or `--no-line-simplification`: Don't simplify lines and polygons
 * `-pS` or `--simplify-only-low-zooms`: Don't simplify lines and polygons at maxzoom (but do simplify at lower zooms)
 * `-pt` or `--no-tiny-polygon-reduction`: Don't combine the area of very small polygons into small squares that represent their combined area.

### Attempts to improve shared polygon boundaries

 * `-ab` or `--detect-shared-borders`: In the manner of [TopoJSON](https://github.com/mbostock/topojson/wiki/Introduction), detect borders that are shared between multiple polygons and simplify them identically in each polygon. This takes more time and memory than considering each polygon individually.
 * `-aL` or `--grid-low-zooms`: At all zoom levels below _maxzoom_, snap all lines and polygons to a stairstep grid instead of allowing diagonals. You will also want to specify a tile resolution, probably `-D8`. This option provides a way to display continuous parcel, gridded, or binned data at low zooms without overwhelming the tiles with tiny polygons, since features will either get stretched out to the grid unit or lost entirely, depending on how they happened to be aligned in the original data. You probably don't want to use this.

### Controlling clipping to tile boundaries

 * `-b` _pixels_ or `--buffer=`_pixels_: Buffer size where features are duplicated from adjacent tiles. Units are "screen pixels"—1/256th of the tile width or height. (default 5)
 * `-pc` or `--no-clipping`: Don't clip features to the size of the tile. If a feature overlaps the tile's bounds or buffer at all, it is included completely. Be careful: this can produce very large tilesets, especially with large polygons.
 * `-pD` or `--no-duplication`: As with `--no-clipping`, each feature is included intact instead of cut to tile boundaries. In addition, it is included only in a single tile per zoom level rather than potentially in multiple copies. Clients of the tileset must check adjacent tiles (possibly some distance away) to ensure they have all features.

### Reordering features within each tile

 * `-pi` or `--preserve-input-order`: Preserve the original input order of features as the drawing order instead of ordering geographically. (This is implemented as a restoration of the original order at the end, so that dot-dropping is still geographic, which means it also undoes `-ao`).
 * `-ao` or `--reorder`: Reorder features to put ones with the same properties in sequence, to try to get them to coalesce. You probably don't want to use this.
 * `-ac` or `--coalesce`: Coalesce adjacent line and polygon features that have the same properties. You probably don't want to use this.
 * `-ar` or `--reverse`: Try reversing the directions of lines to make them coalesce and compress better. You probably don't want to use this.

### Adding calculated attributes

 * `-ag` or `--calculate-feature-density`: Add a new attribute, `tippecanoe_feature_density`, to each feature, to record how densely features are spaced in that area of the tile. You can use this attribute in the style to produce a glowing effect where points are densely packed. It can range from 0 in the sparsest areas to 255 in the densest.

### Trying to correct bad source geometry

 * `-aw` or `--detect-longitude-wraparound`: Detect when adjacent points within a feature jump to the other side of the world, and try to fix the geometry.

### Setting or disabling tile size limits

 * `-M` _bytes_ or `--maximum-tile-bytes=`_bytes_: Use the specified number of _bytes_ as the maximum compressed tile size instead of 500K.
 * `-pf` or `--no-feature-limit`: Don't limit tiles to 200,000 features
 * `-pk` or `--no-tile-size-limit`: Don't limit tiles to 500K bytes
 * `-pC` or `--no-tile-compression`: Don't compress the PBF vector tile data.
 * `-pg` or `--no-tile-stats`: Don't generate the `tilestats` row in the tileset metadata. Uploads without [tilestats](https://github.com/mapbox/mapbox-geostats) will take longer to process.

### Temporary storage

 * `-t` _directory_ or `--temporary-directory=`_directory_: Put the temporary files in _directory_.
   If you don't specify, it will use `/tmp`.

### Progress indicator

 * `-q` or `--quiet`: Work quietly instead of reporting progress or warning messages
 * `-Q` or `--no-progress-indicator`: Don't report progress, but still give warnings
 * `-v` or `--version`: Report Tippecanoe's version number

### Filters

 * `-C` _command_ or `--prefilter=`_command_: Specify a shell filter command to be run at the start of assembling each tile
 * `-c` _command_ or `--postfilter=`_command_: Specify a shell filter command to be run at the end of assembling each tile

The pre- and post-filter commands allow you to do optional filtering or transformation on the features of each tile
as it is created. They are shell commands, run with the zoom level, X, and Y as the `$1`, `$2`, and `$3` arguments.
Future versions of Tippecanoe may add additional arguments for more context.

The features are provided to the filter
as a series of newline-delimited GeoJSON objects on the standard input, and `tippecanoe` expects to read another
set of GeoJSON features from the filter's standard output.

The prefilter receives the features at the highest available resolution, before line simplification,
polygon topology repair, gamma calculation, dynamic feature dropping, or other internal processing.
The postfilter receives the features at tile resolution, after simplification, cleaning, and dropping.

The layer name is provided as part of the `tippecanoe` element of the feature and must be passed through
to keep the feature in its correct layer. In the case of the prefilter, the `tippecanoe` element may also
contain `index`, `sequence`, and `extent` elements, which must be passed through for internal operations like
`--drop-densest-as-needed`, `--drop-smallest-as-needed`, and `--preserve-input-order` to work.

#### Examples:

 * Make a tileset of the Natural Earth countries to zoom level 5, and also copy the GeoJSON features
   to files in a `tiles/z/x/y.geojson` directory hierarchy.

```
tippecanoe -o countries.mbtiles -z5 -C 'mkdir -p tiles/$1/$2; tee tiles/$1/$2/$3.geojson' ne_10m_admin_0_countries.json
```

 * Make a tileset of the Natural Earth countries to zoom level 5, but including only those tiles that
   intersect the [bounding box of Germany](https://www.flickr.com/places/info/23424829).
   (The `limit-tiles-to-bbox` script is [in the Tippecanoe source directory](filters/limit-tiles-to-bbox).)

```
tippecanoe -o countries.mbtiles -z5 -C './filters/limit-tiles-to-bbox 5.8662 47.2702 15.0421 55.0581 $*' ne_10m_admin_0_countries.json
```

 * Make a tileset of TIGER roads in Tippecanoe County, leaving out all but primary and secondary roads (as [classified by TIGER](https://www.census.gov/geo/reference/mtfcc.html)) below zoom level 11.

```
tippecanoe -o roads.mbtiles -c 'if [ $1 -lt 11 ]; then grep "\"MTFCC\": \"S1[12]00\""; else cat; fi' tl_2016_18157_roads.json
```

Environment
-----------

Tippecanoe ordinarily uses as many parallel threads as the operating system claims that CPUs are available.
You can override this number by setting the `TIPPECANOE_MAX_THREADS` environmental variable.

GeoJSON extension
-----------------

Tippecanoe defines a GeoJSON extension that you can use to specify the minimum and/or maximum zoom level
at which an individual feature will be included in the vector tileset being produced.
If you have a feature like this:

```
{
    "type" : "Feature",
    "tippecanoe" : { "maxzoom" : 9, "minzoom" : 4 },
    "properties" : { "FULLNAME" : "N Vasco Rd" },
    "geometry" : {
        "type" : "LineString",
        "coordinates" : [ [ -121.733350, 37.767671 ], [ -121.733600, 37.767483 ], [ -121.733131, 37.766952 ] ]
    }
}
```

with a `tippecanoe` object specifiying a `maxzoom` of 9 and a `minzoom` of 4, the feature
will only appear in the vector tiles for zoom levels 4 through 9. Note that the `tippecanoe`
object belongs to the Feature, not to its `properties`. If you specify a `minzoom` for a feature,
it will be preserved down to that zoom level even if dot-dropping with `-r` would otherwise have
dropped it.

You can also specify a layer name in the `tippecanoe` object, which will take precedence over
the filename or name specified using `--layer`, like this:

```
{
    "type" : "Feature",
    "tippecanoe" : { "layer" : "streets" },
    "properties" : { "FULLNAME" : "N Vasco Rd" },
    "geometry" : {
        "type" : "LineString",
        "coordinates" : [ [ -121.733350, 37.767671 ], [ -121.733600, 37.767483 ], [ -121.733131, 37.766952 ] ]
    }
}
```

Geometric simplifications
-------------------------

At every zoom level, line and polygon features are subjected to Douglas-Peucker
simplification to the resolution of the tile.

For point features, it drops 1/2.5 of the dots for each zoom level above the
point base zoom (which is normally the same as the `-z` max zoom, but can be
a different zoom specified with `-B` if you have precise but sparse data).
I don't know why 2.5 is the appropriate number, but the densities of many different
data sets fall off at about this same rate. You can use -r to specify a different rate.

You can use the gamma option to thin out especially dense clusters of points.
For any area where dots are closer than one pixel together (at whatever zoom level),
a gamma of 3, for example, will reduce these clusters to the cube root of their original density.

For line features, it drops any features that are too small to draw at all.
This still leaves the lower zooms too dark (and too dense for the 500K tile limit,
in some places), so I need to figure out an equitable way to throw features away.

Unless you specify `--no-tiny-polygon-reduction`,
any polygons that are smaller than a minimum area (currently 4 square subpixels) will
have their probability diffused, so that some of them will be drawn as a square of
this minimum size and others will not be drawn at all, preserving the total area that
all of them should have had together.

Features in the same tile that share the same type and attributes are coalesced
together into a single geometry if you use `--coalesce`. You are strongly encouraged to use -x to exclude
any unnecessary properties to reduce wasted file size.

If a tile is larger than 500K, it will try encoding that tile at progressively
lower resolutions before failing if it still doesn't fit.

Development
-----------

Requires sqlite3 and zlib (should already be installed on MacOS). Rebuilding the manpage
uses md2man (`gem install md2man`).

Linux:

    sudo apt-get install build-essential libsqlite3-dev zlib1g-dev

Then build:

    make

and perhaps

    make install

Tippecanoe now requires features from the 2011 C standard. If your compiler is older than
that, you will need to install a newer one. On MacOS, updating to the lastest XCode should
get you a new enough version of `clang`. On Linux, you should be able to upgrade `g` with

```
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update -y
sudo apt-get install -y g-5
export CXX=g-5
```

Docker Image
------------

A tippecanoe Docker image can be built from source and executed as a task to
automatically install dependencies and allow tippecanoe to run on any system
supported by Docker.

```docker
$ docker build -t tippecanoe:latest .
$ docker run -it --rm \
  -v /tiledata:/data \
  tippecanoe:latest \
  tippecanoe --output=/data/output.mbtiles /data/example.geojson
```

The commands above will build a Docker image from the source and compile the
latest version. The image supports all tippecanoe flags and options.

Examples
------

Check out [some examples of maps made with tippecanoe](MADE_WITH.md)

Name
----

The name is [a joking reference](http://en.wikipedia.org/wiki/Tippecanoe_and_Tyler_Too) to a "tiler" for making map tiles.

tile-join
=========

Tile-join is a tool for copying and merging vector mbtiles files and for
joining new attributes from a CSV file to existing features in them.

It reads the tiles from an
existing .mbtiles file or a directory of tiles, matches them against the
records of the CSV (if one is specified), and writes out a new tileset.

If you specify multiple source mbtiles files or source directories of tiles,
all the sources are read and their combined contents are written to the new
mbtiles output. If they define the same layers or the same tiles, the layers
or tiles are merged.

The options are:

### Output tileset

 * `-o` *out.mbtiles* or `--output=`*out.mbtiles*: Write the new tiles to the specified .mbtiles file.
 * `-e` *directory* or `--output-to-directory=`*directory*: Write the new tiles to the specified directory instead of to an mbtiles file.
 * `-f` or `--force`: Remove *out.mbtiles* if it already exists.

### Tileset description and attribution

 * `-A` *attribution* or `--attribution=`*attribution*: Set the attribution string.
 * `-n` *name* or `--name=`*name*: Set the tileset name.
 * `-N` *description* or `--description=`*description*: Set the tileset description.

### Layer filtering and naming

 * `-l` *layer* or `--layer=`*layer*: Include the named layer in the output. You can specify multiple `-l` options to keep multiple layers. If you don't specify, they will all be retained.
 * `-L` *layer* or `--exclude-layer=`*layer*: Remove the named layer from the output. You can specify multiple `-L` options to remove multiple layers.
 * `-R`*old*`:`*new* or `--rename-layer=`*old*`:`*new*: Rename the layer named *old* to be named *new* instead. You can specify multiple `-R` options to rename multiple layers. Renaming happens before filtering.

### Zoom levels

 * `-z` _zoom_ or `--maximum-zoom=`_zoom_: Don't copy tiles from higher zoom levels than the specified zoom
 * `-Z` _zoom_ or `--minimum-zoom=`_zoom_: Don't copy tiles from lower zoom levels than the specified zoom

### Merging attributes from a CSV file

 * `-c` *match*`.csv` or `--csv=`*match*`.csv`: Use *match*`.csv` as the source for new attributes to join to the features. The first line of the file should be the key names; the other lines are values. The first column is the one to match against the existing features; the other columns are the new data to add.

### Filtering features and feature attributes

 * `-x` *key* or `--exclude=`*key*: Remove attributes of type *key* from the output. You can use this to remove the field you are matching against if you no longer need it after joining, or to remove any other attributes you don't want.
 * `-i` or `--if-matched`: Only include features that matched the CSV.
 * `-j` *filter* or `--feature-filter`=*filter*: Check features against a per-layer filter (as defined in the [Mapbox GL Style Specification](https://www.mapbox.com/mapbox-gl-js/style-spec/#types-filter)) and only include those that match. Any features in layers that have no filter specified will be passed through. Filters for the layer `"*"` apply to all layers.
 * `-J` *filter-file* or `--feature-filter-file`=*filter-file*: Like `-j`, but read the filter from a file.

### Setting or disabling tile size limits

 * `-pk` or `--no-tile-size-limit`: Don't skip tiles larger than 500K.
 * `-pC` or `--no-tile-compression`: Don't compress the PBF vector tile data.
 * `-pg` or `--no-tile-stats`: Don't generate the `tilestats` row in the tileset metadata. Uploads without [tilestats](https://github.com/mapbox/mapbox-geostats) will take longer to process.

Because tile-join just copies the geometries to the new .mbtiles without processing them
(except to rescale the extents if necessary),
it doesn't have any of tippecanoe's recourses if the new tiles are bigger than the 500K tile limit.
If a tile is too big and you haven't specified `-pk`, it is just left out of the new tileset.

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
    print "\"" substr($0, 28, 2) substr($0, 30, 3) substr($0, 55, 6) substr($0, 62, 4) "\"," (0  substr($0, 328, 9))
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

Another example
---------------

If you have this GeoJSON `points.json`, with attribute `id`:

```
{ "type": "Feature", "properties": { "id": 1 }, "geometry": { "type": "Point", "coordinates": [ 0, 0 ] } }
{ "type": "Feature", "properties": { "id": 2 }, "geometry": { "type": "Point", "coordinates": [ 1, 1 ] } }
{ "type": "Feature", "properties": { "id": 3 }, "geometry": { "type": "Point", "coordinates": [ 2, 2 ] } }
{ "type": "Feature", "properties": { "id": 4 }, "geometry": { "type": "Point", "coordinates": [ 3, 3 ] } }
{ "type": "Feature", "properties": { "id": 5 }, "geometry": { "type": "Point", "coordinates": [ 4, 4 ] } }
```

and this CSV `join.csv`, with first column `id`:

```
id,name,square
1,One,1
2,Two,4
3,Three,9
4,Four,16
5,Five,25
```

it will add `name` and `square` attributes to each feature based on the matching `id` if you do:

```
tippecanoe -o points.mbtiles points.json
tile-join -o joined.mbtiles -c join.csv points.mbtiles
```

tippecanoe-enumerate
====================

The `tippecanoe-enumerate` utility lists the tiles that an `mbtiles` file defines.
Each line of the output lists the name of the `mbtiles` file and the zoom, x, and y
coordinates of one of the tiles. It does basically the same thing as

    select zoom_level, tile_column, (1 << zoom_level) - 1 - tile_row from tiles;

on the file in sqlite3.

tippecanoe-decode
=================

The `tippecanoe-decode` utility turns vector mbtiles back to GeoJSON. You can use it either
on an entire file:

    tippecanoe-decode file.mbtiles

or on an individual tile:

    tippecanoe-decode file.mbtiles zoom x y
    tippecanoe-decode file.vector.pbf zoom x y

If you decode an entire file, you get a nested `FeatureCollection` identifying each
tile and layer separately. Note that the same features generally appear at all zooms,
so the output for the file will have many copies of the same features at different
resolutions.

### Options

 * `-s` _projection_ or `--projection=`*projection*: Specify the projection of the output data. Currently supported are EPSG:4326 (WGS84, the default) and EPSG:3857 (Web Mercator).
 * `-z` _maxzoom_ or `--maximum-zoom=`*maxzoom*: Specify the highest zoom level to decode from the tileset
 * `-Z` _minzoom_ or `--minimum-zoom=`*minzoom*: Specify the lowest zoom level to decode from the tileset
 * `-l` _layer_ or `--layer=`*layer*: Decode only layers with the specified names. (Multiple `-l` options can be specified.)
 * `-c` or `--tag-layer-and-zoom`: Include each feature's layer and zoom level as part of its `tippecanoe` object rather than as a FeatureCollection wrapper
 * `-S` or `--stats`: Just report statistics about each tile's size and the number of features in it, as a JSON structure.
 * `-f` or `--force`: Decode tiles even if polygon ring order or closure problems are detected

tippecanoe-json-tool
====================

Extracts GeoJSON features or standalone geometries as line-delimited JSON objects from a larger JSON file,
following the same extraction rules that Tippecanoe uses when parsing JSON.

    tippecanoe-json-tool file.json [... file.json]

Optionally also wraps them in a FeatureCollection or GeometryCollection as appropriate.

Optionally extracts an attribute from the GeoJSON `properties` for sorting.

Optionally joins a sorted CSV of new attributes to a sorted GeoJSON file.

The reason for requiring sorting is so that it is possible to work on CSV and GeoJSON files that are larger
than can comfortably fit in memory by streaming through them in parallel, in the same way that the Unix
`join` command does. The Unix `sort` command can be used to sort large files to prepare them for joining.

The sorting interface is weird, and future version of `tippecanoe-json-tool` will replace it with
something better.

### Options

 * `-w` or `--wrap`: Add the FeatureCollection or GeometryCollection wrapper.
 * `-e` *attribute* or `--extract=`*attribute*: Extract the named attribute as a prefix to each feature.
   The formatting makes excessive use of `\u` quoting so that it follows JSON string rules but will still
   be sorted correctly by tools that just do ASCII comparisons.
 * `-c` *file.csv* or `--csv=`*file.csv*: Join properties from the named sorted CSV file, using its first column as the join key. Geometries will be passed through even if they do not match the CSV; CSV lines that do not match a geometry will be discarded.

### Example

Join Census LEHD ([Longitudinal Employer-Household Dynamics](https://lehd.ces.census.gov/)) employment data to a file of Census block geography
for Tippecanoe County, Indiana.

Download Census block geometry, and convert to GeoJSON:

```
$ curl -L -O https://www2.census.gov/geo/tiger/TIGER2010/TABBLOCK/2010/tl_2010_18157_tabblock10.zip
$ unzip tl_2010_18157_tabblock10.zip
$ ogr2ogr -f GeoJSON tl_2010_18157_tabblock10.json tl_2010_18157_tabblock10.shp
```

Download Indiana employment data, and fix name of join key in header

```
$ curl -L -O https://lehd.ces.census.gov/data/lodes/LODES7/in/wac/in_wac_S000_JT00_2015.csv.gz
$ gzip -dc in_wac_S000_JT00_2015.csv.gz | sed '1s/w_geocode/GEOID10/' > in_wac_S000_JT00_2015.csv
```

Sort GeoJSON block geometry so it is ordered by block ID. If you don't do this, you will get a
"GeoJSON file is out of sort" error.

```
$ tippecanoe-json-tool -e GEOID10 tl_2010_18157_tabblock10.json | LC_ALL=C sort > tl_2010_18157_tabblock10.sort.json
```

Join block geometries to employment properties:

```
$ tippecanoe-json-tool -c in_wac_S000_JT00_2015.csv tl_2010_18157_tabblock10.sort.json > blocks-wac.json
```
