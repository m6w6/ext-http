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
--INI--
always_populate_raw_post_data=-1
--FILE--
<?php
echo "Test\n";

use http\env\Request as HttpEnvRequest;

$m = new HttpEnvRequest();

// travis' env headers have another order, wtf?
$h = $m->getHeaders();
ksort($h);
$m->setHeaders($h);

var_dump($m);

echo "Message->toString\n";
echo $m,"\n";

echo "Body->toString\n";
var_dump((string)$m->getBody());

echo "stream\n";
var_dump(file_get_contents("php://input"));
?>
Done
--EXPECTF--
Test
object(%s)#%d (13) {
  ["type":protected]=>
  int(1)
  ["body":protected]=>
  NULL
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
    ["Content-Length"]=>
    string(1) "3"
    ["Content-Type"]=>
    string(14) "test/something"
    ["Cookie"]=>
    string(7) "foo=bar"
    ["X-Test"]=>
    string(4) "test"
  }
  ["parentMessage":protected]=>
  NULL
  ["query":protected]=>
  object(http\QueryString)#%d (1) {
    ["queryArray":"http\QueryString":private]=>
    array(0) {
    }
  }
  ["form":protected]=>
  object(http\QueryString)#%d (1) {
    ["queryArray":"http\QueryString":private]=>
    array(0) {
    }
  }
  ["cookie":protected]=>
  object(http\QueryString)#%d (1) {
    ["queryArray":"http\QueryString":private]=>
    array(1) {
      ["foo"]=>
      string(3) "bar"
    }
  }
  ["files":protected]=>
  array(0) {
  }
}
Message->toString
POST / HTTP/1.1%a
Content-Length: 3%a
Content-Type: test/something%a
Cookie: foo=bar%a
X-Test: test%a
%a
b=c
Body->toString
string(3) "b=c"
stream
string(3) "b=c"
Done
