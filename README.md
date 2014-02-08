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

