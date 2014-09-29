tippecanoe
==========

Build vector tilesets from large collections of GeoJSON features.

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

 * -l Layer name (default "file" if source is file.json)
 * -n Human-readable name (default file.json)
 * -z Base zoom level (default 14)
 * -Z Lowest zoom level (default 0)
 * -d Detail at base zoom level (default 12, for tile resolution of 4096)
 * -D Detail at lower zoom levels (default 10, for tile resolution of 1024) 
 * -x Property (removes the named properties from all features)
 * -f Force: Delete existing mbtiles file if it already exists.

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
data sets fall off at about this same rate.

For line features, it drops any features that are too small to draw at all.
This still leaves the lower zooms too dark (and too dense for the 500K tile limit,
in some places), so I need to figure out an equitable way to throw features away.

It also throws away any polygons that are too small to draw. I'm not sure yet
if it is appropriate to do more than that.

Features in the same tile that share the same type and attributes are coalesced
together into a single geometry.

Development
-----------

Requires protoc (brew install protobuf or apt-get install libprotobuf-dev),
and sqlite3 (apt-get install libsqlite3-dev). To build:

    make

and perhaps

    make install
