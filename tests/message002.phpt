--TEST--
env request message
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

echo "Message->toString\n";
echo $m,"\n";

echo "Body->toString\n";
var_dump((string)$m->getBody());

echo "stream\n";
var_dump(file_get_contents("php://input"));

echo "Done\n";
--EXPECTF--
Test
object(%s)#%d (12) {
  ["type":protected]=>
  int(1)
  ["body":protected]=>
  object(http\Message\Body)#%d (0) {
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
  object(http\QueryString)#2 (1) {
    ["queryArray":"http\QueryString":private]=>
    array(0) {
    }
  }
  ["form":protected]=>
  object(http\QueryString)#3 (1) {
    ["queryArray":"http\QueryString":private]=>
    array(0) {
    }
  }
  ["files":protected]=>
  array(0) {
  }
}
Message->toString
POST / HTTP/1.1%a
X-Test: test%a
Content-Length: 3%a
Content-Type: test/something%a
Cookie: foo=bar%a
%a
b=c
Body->toString
string(3) "b=c"
stream
string(3) "b=c"
Done
