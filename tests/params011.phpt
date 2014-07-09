--TEST--
bool args params
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$p = new http\Params;
$container = array("value" => false, "arguments" => array("wrong" => false, "correct" => true));
$p["container"] = $container;
var_dump("container=0;wrong=0;correct" === $p->toString());
var_dump(array("container" => $container) === $p->toArray());

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
