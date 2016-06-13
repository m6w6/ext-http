--TEST--
header negotiation
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

function r($v) { return round($v, 2); }

$a = new http\Header("Accept", "text/html, text/plain;q=0.5, */*;q=0");
var_dump("text/html" === $a->negotiate(array("text/plain","text/html")));
var_dump("text/html" === $a->negotiate(array("text/plain","text/html"), $rs));
var_dump(array("text/html"=>0.99, "text/plain"=>0.5) === array_map("r", $rs));
var_dump("text/plain" === $a->negotiate(array("foo/bar", "text/plain"), $rs));
var_dump(array("text/plain"=>0.5) === array_map("r", $rs));
var_dump("foo/bar" === $a->negotiate(array("foo/bar"), $rs));
var_dump(array() === $rs);

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Done
