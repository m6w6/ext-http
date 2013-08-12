--TEST--
header serialize
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$h = new http\Header("foo", "bar");
var_dump("Foo: bar" === (string) unserialize(serialize($h)));
$h = new http\Header(123, 456);
var_dump("123: 456" === (string) unserialize(serialize($h)));

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
Done
