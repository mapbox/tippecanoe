json-pull
=========

A streaming JSON pull parser in C.

Does the world really need another JSON parser? Maybe not.
But what distinguishes this one is the desire to let you
read through gigantic files larger than you can comfortably
fit in memory and still find the parts you need.

Setup
-----

You can create a <code>json_pull</code> parser object
by calling <code>json_begin_file()</code> to begin reading from a file
or <code>json_begin_string()</code> to begin reading from a string.

You can also read from anything you want by calling <code>json_begin()</code>
with your own <code>read()</code> and <code>peek()</code> functions that
return or preview, respectively, the next UTF-8 byte from your input stream.

Reading single JSON objects
---------------------------

The simplest form is <code>json_read_tree()</code>, which reads a complete
JSON object from the stream, or NULL if there was an error or on end of file.
Call <code>json_free()</code> on the object when you are done with it,
and <code>json_end()</code> on the parser.

You can continue calling <code>json_read_tree()</code> to read additional objects
from the same stream. This is not standard JSON, but is useful for something like
the Twitter filter stream that contains a series of JSON objects separated by
newlines, without a wrapper array that contains them all.

Reading JSON streams
--------------------

With something like GeoJSON, where it is common to have a large wrapper array
that contains many smaller items that are often useful to consider separately,
you can read the stream one token at a time.

The normal form is <code>json_read()</code>, which returns the next complete
object from the stream. This can be a single string, number, <code>true</code>,
<code>false</code>, or <code>null</code>, or it can be an array or hash that
contains other primitive or compound objects.

Note that each array or hash will be returned following all the objects that it contains.
The outermost object will be the same one that <code>json_read_tree()</code> would
have returned, and you can tell that it is the outer object because its
<code>parent</code> field is null.

You can call <code>json_free()</code> on each object as you are finished with it,
or wait until the end and call <code>json_free()</code> on the outer object
which will recursively free everything that it contains. Freeing an object before
its container is complete also removes it from its parent array or hash so that
there are not dangling references left to it.

Reading JSON streams with callbacks
-----------------------------------

If you are outputting a new stream as you read instead of just looking for the
sub-objects that interest you,
