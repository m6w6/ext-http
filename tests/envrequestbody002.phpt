--TEST--
env request body
--SKIPIF--
<? include "skipif.inc";
--PUT--
Content-Type: application/x-www-form-urlencoded
foo=bar&baz=buh
--FILE--
<?
var_dump($_POST);
?>
DONE
--EXPECT--
array(2) {
  ["foo"]=>
  string(3) "bar"
  ["baz"]=>
  string(3) "buh"
}
DONE
