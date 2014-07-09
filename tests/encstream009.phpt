--TEST--
encoding stream zlib error
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

var_dump(http\Encoding\Stream\Inflate::decode("if this goes through, something's pretty wrong"));

?>
DONE
--EXPECTF--
Test

Warning: http\Encoding\Stream\Inflate::decode(): Could not inflate data: data error in %s on line %d
bool(false)
DONE
