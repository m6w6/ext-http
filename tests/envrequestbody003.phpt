--TEST--
env request body
--SKIPIF--
<? include "skipif.inc";
--PUT--
Content-Type: multipart/form-data;boundary=123
--123
Content-Disposition: form-data; name="foo"

bar
--123
Content-Disposition: form-data; name="baz"

buh
--123
Content-Disposition: form-data; name="up"; filename="up.txt"

foo=bar&baz=buh
--123--
--FILE--
<?
var_dump($_POST);
var_dump($_FILES);
?>
DONE
--EXPECTF--
array(2) {
  ["foo"]=>
  string(3) "bar"
  ["baz"]=>
  string(3) "buh"
}
array(1) {
  ["up"]=>
  array(5) {
    ["name"]=>
    string(6) "up.txt"
    ["type"]=>
    string(0) ""
    ["tmp_name"]=>
    string(14) "%s"
    ["error"]=>
    int(0)
    ["size"]=>
    int(15)
  }
}
DONE
