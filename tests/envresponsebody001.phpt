--TEST--
env response body
--SKIPIF--
<?php
include "skipif.inc";
?>
--INI--
output_buffering=1
--FILE--
<?php
echo "Test\n";
$r = new http\Env\Response;
var_dump((string) $r->getBody());
?>
Done
--EXPECTF--
Test
string(5) "Test
"
Done
