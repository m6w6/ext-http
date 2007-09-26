--TEST--
parse cookie
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";

var_dump(http_parse_cookie('name="value"; foo="bar\"baz"; hey=got"it ; path=/     ; comment=; expires='.http_date(1).' secure ; httpOnly', 0, array("comment")));

echo "Done\n";
?>
--EXPECTF--
%sTEST
object(stdClass)%s {
  ["cookies"]=>
  array(3) {
    ["name"]=>
    string(5) "value"
    ["foo"]=>
    string(7) "bar"baz"
    ["hey"]=>
    string(6) "got"it"
  }
  ["extras"]=>
  array(1) {
    ["comment"]=>
    string(0) ""
  }
  ["flags"]=>
  int(32)
  ["expires"]=>
  int(1)
  ["path"]=>
  string(1) "/"
  ["domain"]=>
  string(0) ""
}
Done
