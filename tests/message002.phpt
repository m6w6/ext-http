--TEST--
env request Message
--SKIPIF--
<?php include "skipif.inc"; ?>
--POST_RAW--
Content-Type: test/something
b=c
--ENV--
HTTP_X_TEST=test
--COOKIE--
foo=bar
--FILE--
<?
echo "Test\n";

use http\env\Request as HttpEnvRequest;

$m = new HttpEnvRequest();

var_dump($m);
echo $m,"\n";

var_dump((string)$m->getBody());

echo "Done\n";
--EXPECTF--
Test
object(%s)#%d (13) {
  ["errorHandling":protected]=>
  NULL
  ["type":protected]=>
  int(1)
  ["body":protected]=>
  object(%s)#%d (1) {
    ["errorHandling":protected]=>
    NULL
  }
  ["requestMethod":protected]=>
  string(4) "POST"
  ["requestUrl":protected]=>
  string(0) ""
  ["responseStatus":protected]=>
  string(0) ""
  ["responseCode":protected]=>
  int(0)
  ["httpVersion":protected]=>
  string(3) "1.1"
  ["headers":protected]=>
  array(4) {
    ["X-Test"]=>
    string(4) "test"
    ["Content-Length"]=>
    string(1) "3"
    ["Content-Type"]=>
    string(14) "test/something"
    ["Cookie"]=>
    string(7) "foo=bar"
  }
  ["parentMessage":protected]=>
  NULL
  ["query":protected]=>
  object(http\QueryString)#2 (2) {
    ["errorHandling":protected]=>
    NULL
    ["queryArray":"http\QueryString":private]=>
    array(0) {
    }
  }
  ["form":protected]=>
  object(http\QueryString)#3 (2) {
    ["errorHandling":protected]=>
    NULL
    ["queryArray":"http\QueryString":private]=>
    array(0) {
    }
  }
  ["files":protected]=>
  array(0) {
  }
}
POST / HTTP/1.1%a
X-Test: test%a
Content-Length: 3%a
Content-Type: test/something%a
Cookie: foo=bar%a
%a
b=c
string(3) "b=c"
Done
