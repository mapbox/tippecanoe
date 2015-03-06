tippecanoe
==========

Build vector tilesets from large collections of GeoJSON features.

Installation
------------

The easiest way to install tippecanoe on OSX is with [Homebrew](http://brew.sh/):

    brew install tippecanoe

Usage
-----

    tippecanoe -o file.mbtiles [file.json]

If the file is not specified, it reads GeoJSON from the standard input.

The GeoJSON features need not be wrapped in a FeatureCollection.
You can concatenate multiple GeoJSON features or files together,
and it will parse out the features and ignore whatever other objects
it encounters.

Options
-------

 * -l <i>name</i>: Layer name (default "file" if source is file.json)
 * -n <i>name</i>: Human-readable name (default file.json)
 * -z <i>zoom</i>: Base zoom level (default 14)
 * -Z <i>zoom</i>: Lowest zoom level (default 0)
 * -d <i>detail</i>: Detail at base zoom level (default 26-basezoom, ~0.5m, for tile resolution of 4096 if -z14)
 * -D <i>detail</i>: Detail at lower zoom levels (default 10, for tile resolution of 1024) 
 * -x <i>name</i>: Exclude the named properties from all features
 * -y <i>name</i>: Include the named properties in all features, excluding all those not explicitly named
 * -X: Exclude all properties and encode only geometries
 * -f: Delete the mbtiles file if it already exists instead of giving an error
 * -r <i>rate</i>: Rate at which dots are dropped at lower zoom levels (default 2.5)
 * -b <i>pixels</i>: Buffer size where features are duplicated from adjacent tiles (default 5)
 * -g <i>gamma</i>: Rate at which especially dense dots are dropped (default 1, for no effect). A gamma of 2 reduces the number of dots duplicating the same pixel to the square root of their original number.

Example
-------

    tippecanoe -o alameda.mbtiles -l alameda -n "Alameda County from TIGER" -z13 tl_2014_06001_roads.json

    cat tiger/tl_2014_*_roads.json | tippecanoe -o tiger.mbtiles -l roads -n "All TIGER roads, one zoom" -z12 -Z12 -d14 -x LINEARID -x RTTYP

Geometric simplifications
-------------------------

At every zoom level, line and polygon features are subjected to Douglas-Peucker
simplification to the resolution of the tile.

For point features, it drops 1/2.5 of the dots for each zoom level above the base.
I don't know why 2.5 is the appropriate number, but the densities of many different
data sets fall off at about this same rate. You can use -r to specify a different rate.

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

Requires protoc (brew install protobuf or apt-get install libprotobuf-dev),
and sqlite3 (apt-get install libsqlite3-dev). To build:

    make

and perhaps

    make install
