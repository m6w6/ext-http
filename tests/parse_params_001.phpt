--TEST--
http_parse_params
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";
var_dump(http_parse_params('text/html; charset=iso-8859-1'));
var_dump(http_parse_params('text/html; charset="iso-8859-1"'));
var_dump(http_parse_params('attachment; filename="gol;got,a.ext"'));
var_dump(http_parse_params('public, must-revalidate, max-age=0'));
var_dump(http_parse_params('a')->params[0]);
var_dump(http_parse_params('a=b')->params[0]);
echo "Done\n";
--EXPECTF--
%sTEST
object(stdClass)#%d (%d) {
  ["params"]=>
  array(2) {
    [0]=>
    string(9) "text/html"
    [1]=>
    array(1) {
      ["charset"]=>
      string(10) "iso-8859-1"
    }
  }
}
object(stdClass)#%d (%d) {
  ["params"]=>
  array(2) {
    [0]=>
    string(9) "text/html"
    [1]=>
    array(1) {
      ["charset"]=>
      string(10) "iso-8859-1"
    }
  }
}
object(stdClass)#%d (%d) {
  ["params"]=>
  array(2) {
    [0]=>
    string(10) "attachment"
    [1]=>
    array(1) {
      ["filename"]=>
      string(13) "gol;got,a.ext"
    }
  }
}
object(stdClass)#%d (%d) {
  ["params"]=>
  array(3) {
    [0]=>
    string(6) "public"
    [1]=>
    string(15) "must-revalidate"
    [2]=>
    array(1) {
      ["max-age"]=>
      string(1) "0"
    }
  }
}
string(1) "a"
array(1) {
  ["a"]=>
  string(1) "b"
}
Done
