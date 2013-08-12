--TEST--
header string
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$h = new http\Header("foo", "bar");
var_dump("Foo: bar" === (string) $h);

?>
Done
--EXPECT--
Test
bool(true)
Done
