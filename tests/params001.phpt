--TEST--
header params
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$ct = new http\Params("text/html; charset=utf-8", ",", ";", "=", 0);

var_dump(
	isset($ct["text/html"]),
	isset($ct["text/json"]),
	$ct["text/html"]["arguments"]["charset"]
);

unset($ct["text/html"]);
$ct[] = "text/json";
$ct["text/json"] = array("arguments" => array("charset" => "iso-8859-1"));

var_dump(
	isset($ct["text/html"]),
	isset($ct["text/json"]),
	$ct["text/json"]["arguments"]["charset"]
);

var_dump((string) $ct,$ct);

?>
DONE
--EXPECTF--
Test
bool(true)
bool(false)
string(5) "utf-8"
bool(false)
bool(true)
string(10) "iso-8859-1"
string(%d) "text/json;charset=iso-8859-1"
object(http\Params)#%d (5) {
  ["params"]=>
  array(1) {
    ["text/json"]=>
    array(2) {
      ["value"]=>
      bool(true)
      ["arguments"]=>
      array(1) {
        ["charset"]=>
        string(10) "iso-8859-1"
      }
    }
  }
  ["param_sep"]=>
  string(1) ","
  ["arg_sep"]=>
  string(1) ";"
  ["val_sep"]=>
  string(1) "="
  ["flags"]=>
  int(0)
}
DONE
