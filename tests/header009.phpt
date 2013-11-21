--TEST--
header params w/ args
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$header = new http\Header("Custom", '"foo" is "bar". "bar" is "bis" where "bis" is "where".');
var_dump(
		array(
				"foo" => array("value" => "bar", "arguments" => array()),
				"bar" => array("value" => "bis", "arguments" => array("bis" => "where"))
		) === $header->getParams(".", "where", "is")->params
);

?>
Done
--EXPECT--
Test
bool(true)
Done
