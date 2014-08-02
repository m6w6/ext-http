--TEST--
default params
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$s = "foo, bar;arg=0;bla, gotit=0;now";
$p = new http\Params($s);
$c = str_replace(" ", "", $s);
$k = array("foo", "bar", "gotit");
$a = array("foo"=>"arg", "bar"=>"bla", "gotit"=>"now");
$r = array (
	'foo' => 
	array (
		'value' => true,
		'arguments' => 
		array (
		),
	),
	'bar' => 
	array (
		'value' => true,
		'arguments' => 
		array (
			'arg' => '0',
			'bla' => true,
		),
	),
	'gotit' => 
	array (
		'value' => '0',
		'arguments' => 
		array (
			'now' => true,
		),
	),
);

# ---

var_dump(count($p->params));

echo "key exists\n";
foreach ($k as $key) {
	var_dump(array_key_exists($key, $p->params));
}

echo "values\n";
foreach ($k as $key) {
	var_dump($p[$key]["value"]);
}

echo "args\n";
foreach ($k as $key) {
	var_dump(count($p[$key]["arguments"]));
}

echo "arg values\n";
foreach ($k as $key) {
	var_dump(@$p[$key]["arguments"][$a[$key]]);
}

echo "equals\n";
var_dump($c === (string) $p);
var_dump($r === $p->params);
$x = new http\Params($p->params);
var_dump($r === $x->toArray());
?>
DONE
--EXPECT--
Test
int(3)
key exists
bool(true)
bool(true)
bool(true)
values
bool(true)
bool(true)
string(1) "0"
args
int(0)
int(2)
int(1)
arg values
NULL
bool(true)
bool(true)
equals
bool(true)
bool(true)
bool(true)
DONE
