--TEST--
empty params
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";
$p = new http\Params(NULL);
var_dump(array() === $p->params);
?>
DONE
--EXPECT--
Test
bool(true)
DONE
