--TEST--
header parse error
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$header = "wass\nup";
http\Header::parse($header);

?>
Done
--EXPECTF--
Test

Warning: http\Header::parse(): Failed to parse headers: unexpected end of line at pos 4 of 'wass\nup' in %s on line %d
Done
