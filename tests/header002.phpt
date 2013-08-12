--TEST--
header numeric
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$h = new http\Header(123, 456);
var_dump("123: 456" === (string) $h);

?>
Done
--EXPECT--
Test
bool(true)
Done
