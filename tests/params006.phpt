--TEST--
escaped params
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$p = new http\Params("form-data; name=\"upload\"; filename=\"trick\\\"\0\\\"ed\"");
$c = array(
	"form-data" => array(
		"value" => true,
		"arguments" => array(
			"name" => "upload",
			"filename" => "trick\"\0\"ed"
		)
	)
);
var_dump($c === $p->params);
var_dump("form-data;name=upload;filename=\"trick\\\"\\000\\\"ed\"" === (string) $p);
?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
