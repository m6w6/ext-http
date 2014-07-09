--TEST--
int key params
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$p = new http\Params("0=nothing;1=yes");
var_dump(array("0" => array("value" => "nothing", "arguments" => array(1=>"yes"))) === $p->params);
var_dump("0=nothing;1=yes" === $p->toString());

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
