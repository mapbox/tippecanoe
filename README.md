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
Call <code>json_free()</code> on the object when you are done with it.

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
sub-objects that interest you, you also need to know when arrays and hashes begin,
not just when they end, so you can output the opening bracket or brace. For this
purpose there is an additional streaming reader function,
<code>json_read_separators()</code>, which takes an additional argument for
a function to call when brackets, braces, commas, and colons are read.
Other object types and the closing of arrays and hashes are still sent through
the normal return value.

The types that can be sent to the callback function are
<code>JSON_ARRAY</code>, <code>JSON_HASH</code>, <code>JSON_COMMA</code>,
and <code>JSON_COLON</code>.

Cleanup
-------

If there was an error while parsing, the parser will have returned NULL before
all the containers were closed. You will probably want to call <code>json_free()</code>
on the <code>root</code> elemement of the parser, which should contain the full parse
tree so far, to avoid leaking memory.

Shutdown
--------

To free the parser object, call <code>json_end()</code> on it.

Object format
-------------

JSON objects are represented as the C struct <code>json_object</code>.
It contains a <code>type</code> field to indicate its type, a <code>parent</code>
pointer to the container that encloses it, and additional fields per type.

Types <code>JSON_NULL</code>, <code>JSON_TRUE</code>, and <code>JSON_FALSE</code>
have no additional data.

Strings have type <code>JSON_STRING</code>, with null-terminated UTF-8 text
in <code>string</code> and length in <code>length</code>.

Numbers have type <code>JSON_NUMBER</code>, with value in <code>number</code>.

Arrays have type <code>JSON_ARRAY</code>. There are <code>length</code> elements in the array,
and the elements are in <code>array</code>.

Hashes have type <code>JSON_HASH</code>. There are <code>length</code> key-value pairs,
and the keys are in <code>keys</code> and the values in <code>values</code>.

Parser format
-------------

The parser object has two fields of public interest: <code>error</code> is a string
describing any errors found in the JSON, and <code>line</code> is the current line number
being read from the input to make it easier to find the errors.

The <code>root</code> field points to the outer object of the current parse tree.

Utility function
----------------

There is a function <code>json_hash_get</code> that looks up the JSON object hash value
corresponding to a C string hash key in a JSON hash object. If the object specified is
NULL or not a JSON hash or has no matching key, it returns NULL.
