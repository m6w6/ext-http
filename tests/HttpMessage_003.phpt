--TEST--
HttpMessage implements Serializable, Countable
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
?>
--FILE--
<?php
echo "-TEST\n";

$m = new HttpMessage(
	"HTTP/1.1 301\r\n".
	"Location: /anywhere\r\n".
	"HTTP/1.1 302\r\n".
	"Location: /somewhere\r\n".
	"HTTP/1.1 200\r\n".
	"Transfer-Encoding: chunked\r\n".
	"\r\n".
	"01\r\n".
	"X\r\n".
	"00"
);

var_dump($m->count());
var_dump($m->serialize());
$m->unserialize("HTTP/1.1 200 Ok\r\nServer: Funky/1.0");
var_dump($m);
var_dump($m->count());

echo "Done\n";
?>
--EXPECTF--
%sTEST
int(3)
string(148) "HTTP/1.1 301
Location: /anywhere
HTTP/1.1 302
Location: /somewhere
HTTP/1.1 200
X-Original-Transfer-Encoding: chunked
Content-Length: 1

X
"
object(HttpMessage)#%d (%d) {
  ["type:protected"]=>
  int(2)
  ["body:protected"]=>
  string(0) ""
  ["requestMethod:protected"]=>
  string(0) ""
  ["requestUrl:protected"]=>
  string(0) ""
  ["responseStatus:protected"]=>
  string(2) "Ok"
  ["responseCode:protected"]=>
  int(200)
  ["httpVersion:protected"]=>
  float(1.1)
  ["headers:protected"]=>
  array(1) {
    ["Server"]=>
    string(9) "Funky/1.0"
  }
  ["parentMessage:protected"]=>
  NULL
}
int(1)
Done
