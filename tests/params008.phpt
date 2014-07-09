--TEST--
querystring params
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$s = "foo=b%22r&bar=b%22z&a%5B%5D%5B%5D=1";
$p = new http\Params($s, "&", "", "=", http\Params::PARSE_QUERY);
$c = array(
	"foo" => array(
		"value" => "b\"r",
		"arguments" => array(),
	),
	"bar" => array(
		"value" => "b\"z",
		"arguments" => array(),
	),
	"a" => array(
		"value" => array(
			array("1")
		),
		"arguments" => array(),
	),
);
var_dump($c === $p->params);
var_dump("foo=b%22r&bar=b%22z&a%5B0%5D%5B0%5D=1" === (string) $p);
?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
