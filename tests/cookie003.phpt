--TEST--
cookies numeric keys
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$c = new http\Cookie("1=%20; 2=%22; 3=%5D", 0, array(2));
var_dump("1=%20; 3=%5D; 2=%22; " === (string) $c);

?>
DONE
--EXPECT--
Test
bool(true)
DONE
