--TEST--
message body clone
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$file = new http\Message\Body(fopen(__FILE__,"r"));
var_dump((string) $file === (string) clone $file);

?>
DONE
--EXPECT--
Test
bool(true)
DONE
