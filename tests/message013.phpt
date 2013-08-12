--TEST--
message detach
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$m = new http\Message(
	"HTTP/1.1 200 Ok\r\n".
	"HTTP/1.1 201 Created\n".
	"HTTP/1.1 302 Found\r\n"
);

var_dump(3 === count($m));
$d = $m->detach();
var_dump(3 === count($m));
var_dump(1 === count($d));

var_dump("HTTP/1.1 302 Found\r\n\r\n" === $d->toString(true));

?>
Done
--EXPECTF--
Test
bool(true)
bool(true)
bool(true)
bool(true)
Done
