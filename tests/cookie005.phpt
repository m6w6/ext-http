--TEST--
cookies simple data
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$orig = new http\Cookie("key=value");
$copy = clone $orig;
$same = new http\Cookie($copy);
$even = new http\Cookie($same->toArray());
foreach (array($orig, $copy) as $c) {
	var_dump($c->getCookie("key"));
	var_dump($c->getExpires());
	var_dump($c->getMaxAge());
	var_dump($c->getFlags());
	var_dump($c->getPath());
	var_dump($c->getDomain());
	var_dump($c->getExtras());
	var_dump($c->getCookies());
	var_dump($c->toString());
	var_dump(
		array (
			"cookies" => 
				array (
					"key" => "value",
				),
			"extras" => 
				array (
				),
			"flags" => 0,
			"expires" => -1,
			"path" => "",
			"domain" => "",
			"max-age" => -1,
		) == $c->toArray()
	);
}

?>
DONE
--EXPECT--
Test
string(5) "value"
int(-1)
int(-1)
int(0)
NULL
NULL
array(0) {
}
array(1) {
  ["key"]=>
  string(5) "value"
}
string(11) "key=value; "
bool(true)
string(5) "value"
int(-1)
int(-1)
int(0)
NULL
NULL
array(0) {
}
array(1) {
  ["key"]=>
  string(5) "value"
}
string(11) "key=value; "
bool(true)
DONE
