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
/* this generates two warnings because of the LF ;) */
http\Header::parse($header);

?>
Done
--EXPECTF--
Test

Warning: http\Header::parse(): Could not parse headers in %s on line %d

Warning: http\Header::parse(): Could not parse headers in %s on line %d
Done
