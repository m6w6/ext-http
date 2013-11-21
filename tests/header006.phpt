--TEST--
header parsing
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$header = "Foo: bar\nBar: foo\n";
var_dump(array("Foo"=>"bar","Bar"=>"foo") === http\Header::parse($header));
 
$headers = http\Header::parse($header, "http\\Header");
var_dump(2 === count($headers));
var_dump(2 === array_reduce($headers, function($count, $header) {
	return $count + ($header instanceof http\Header);
}, 0));
?>
Done
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
Done
