--TEST--
query parser
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
$p = new http\Params("foo=bar&arr[]=1&arr[]=2", array("&",";"), "", "=", http\Params::PARSE_QUERY);

var_dump($p); 

echo $p, "\n";
?>
DONE
--EXPECTF--
object(http\Params)#%d (6) {
  ["errorHandling":protected]=>
  NULL
  ["params"]=>
  array(2) {
    ["foo"]=>
    array(2) {
      ["value"]=>
      string(3) "bar"
      ["arguments"]=>
      array(0) {
      }
    }
    ["arr"]=>
    array(2) {
      ["value"]=>
      array(2) {
        [0]=>
        string(1) "1"
        [1]=>
        string(1) "2"
      }
      ["arguments"]=>
      array(0) {
      }
    }
  }
  ["param_sep"]=>
  array(2) {
    [0]=>
    string(1) "&"
    [1]=>
    string(1) ";"
  }
  ["arg_sep"]=>
  string(0) ""
  ["val_sep"]=>
  string(1) "="
  ["flags"]=>
  int(12)
}
foo=bar&arr%5B0%5D=1&arr%5B1%5D=2
DONE

