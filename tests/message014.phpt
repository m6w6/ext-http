--TEST--
message prepend
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

for ($i=0; $i<9; ++$i) {
	$a[] = new http\Message("HTTP/1.1 ".($i+200));
}

foreach ($a as $m) {
	if (isset($p)) $m->prepend($p);
	$p = $m;
}

var_dump(
	"HTTP/1.1 200\r\n\r\n".
	"HTTP/1.1 201\r\n\r\n".
	"HTTP/1.1 202\r\n\r\n".
	"HTTP/1.1 203\r\n\r\n".
	"HTTP/1.1 204\r\n\r\n".
	"HTTP/1.1 205\r\n\r\n".
	"HTTP/1.1 206\r\n\r\n".
	"HTTP/1.1 207\r\n\r\n".
	"HTTP/1.1 208\r\n\r\n" ===
	$m->toString(true)
);

?>
Done
--EXPECTF--
Test
bool(true)
Done
