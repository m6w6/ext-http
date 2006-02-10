--TEST--
parse cookie
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";

var_dump(http_parse_cookie('name="value"; foo="bar\"baz"; hey=got\"it'));

echo "Done\n";
?>
--EXPECTF--
%sTEST
object(stdClass)#1 (4) {
  ["name"]=>
  string(4) "name"
  ["value"]=>
  string(5) "value"
  ["foo"]=>
  string(7) "bar"baz"
  ["hey"]=>
  string(7) "got\"it"
}
Done
