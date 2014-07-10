--TEST--
cookies raw
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$c = new http\Cookie("1=%20; 2=%22; e3=%5D", http\Cookie::PARSE_RAW, array(2));
var_dump("1=%2520; e3=%255D; 2=%2522; " === (string) $c);

?>
DONE
--EXPECT--
Test
bool(true)
DONE
