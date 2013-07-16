--TEST--
client request
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$r = new http\Client\Request(
	$m = "POST", 
	$u = "http://localhost/foo", 
	$h = array("header", "value"),
    $b = new http\Message\Body(fopen(__FILE__, "r+b"))
);

var_dump($b === $r->getBody());
var_dump($h === $r->getHeaders());
var_dump($u === $r->getRequestUrl());
var_dump($m === $r->getRequestMethod());

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
bool(true)
Done
