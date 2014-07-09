--TEST--
no args params
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$p = new http\Params;
$p["param"] = true;
var_dump("param" === $p->toString());
$p["param"] = false;
var_dump("param=0" === $p->toString());
?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
