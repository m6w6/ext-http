--TEST--
env request cookie
--SKIPIF--
<?php 
include "skipif.inc";
?>
--COOKIE--
foo=bar;bar=123
--FILE--
<?php 
echo "Test\n";

$r = new http\Env\Request;
var_dump($r->getCookie()->toArray());
var_dump($r->getCookie("foo", "s"));
var_dump($r->getCookie("bar", "i"));
var_dump($r->getCookie("baz", "b", true));

?>
DONE
--EXPECT--
Test
array(2) {
  ["foo"]=>
  string(3) "bar"
  ["bar"]=>
  string(3) "123"
}
string(3) "bar"
int(123)
bool(true)
DONE
