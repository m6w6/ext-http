--TEST--
quoted params
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$p = new http\Params("multipart/form-data; boundary=\"--123\"");
$c = array(
	"multipart/form-data" => array(
		"value" => true,
		"arguments" => array(
			"boundary" => "--123"
		)
	)
);
var_dump($c === $p->params);
var_dump("multipart/form-data;boundary=--123" === (string) $p);
?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
