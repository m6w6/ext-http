--TEST--
Bug #69076 (URL parsing throws exception on empty query string)
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 
echo "Test\n";
echo new http\Url("http://foo.bar/?");
?>

===DONE===
--EXPECT--
Test
http://foo.bar/
===DONE===
