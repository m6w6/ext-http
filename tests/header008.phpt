--TEST--
header params
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$header = new http\Header("Cache-control", "public, must-revalidate, max-age=0");
var_dump(
	array(
		"public" => array("value" => true, "arguments" => array()),
		"must-revalidate" => array("value" => true, "arguments" => array()),
		"max-age" => array("value" => "0", "arguments" => array()),
	) === $header->getParams()->params
);

?>
Done
--EXPECT--
Test
bool(true)
Done
