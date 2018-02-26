--TEST--
encoding stream brotli error
--SKIPIF--
<?php
include "skipif.inc";
class_exists("http\\Encoding\\Stream\\Enbrotli") or die("SKIP need brotli support");
?>
--FILE--
<?php
echo "Test\n";

var_dump(http\Encoding\Stream\Debrotli::decode("if this goes through, something's pretty wrong"));

?>
DONE
--EXPECTF--
Test

Warning: http\Encoding\Stream\Debrotli::decode(): Could not brotli decode data: %s in %s on line %d
bool(false)
DONE
