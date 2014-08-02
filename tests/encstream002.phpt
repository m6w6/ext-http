--TEST--
encoding stream chunked not encoded
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$s = "this is apparently not encodded\n";
var_dump($s === http\Encoding\Stream\Dechunk::decode($s));

?>
DONE
--EXPECTF--
Test

Notice: http\Encoding\Stream\Dechunk::decode(): Data does not seem to be chunked encoded in %s on line %d
bool(true)
DONE
