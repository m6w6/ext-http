--TEST--
message var_dump with inherited property with increased access level
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

class c extends http\Message {
	public $headers;
} 

$m = new c;
$m->headers["foo"] = "bar";
var_dump($m);

?>
DONE
--EXPECTF--
Test
object(c)#%d (10) {
  ["headers"]=>
  array(1) {
    ["foo"]=>
    string(3) "bar"
  }
  ["errorHandling":protected]=>
  NULL
  ["type":protected]=>
  int(0)
  ["body":protected]=>
  object(http\Message\Body)#%d (1) {
    ["errorHandling":protected]=>
    NULL
  }
  ["requestMethod":protected]=>
  string(0) ""
  ["requestUrl":protected]=>
  string(0) ""
  ["responseStatus":protected]=>
  string(0) ""
  ["responseCode":protected]=>
  int(0)
  ["httpVersion":protected]=>
  string(3) "1.1"
  ["parentMessage":protected]=>
  NULL
}
DONE
