--TEST--
HttpMessage implements Serializable, Countable
--SKIPIF--
<?php
include 'skip.inc';
checkmin("5.2.5");
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
%aTEST
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
  ["type%s]=>
  int(2)
  ["body%s]=>
  string(0) ""
  ["requestMethod%s]=>
  string(0) ""
  ["requestUrl%s]=>
  string(0) ""
  ["responseStatus%s]=>
  string(2) "Ok"
  ["responseCode%s]=>
  int(200)
  ["httpVersion%s]=>
  float(1.1)
  ["headers%s]=>
  array(1) {
    ["Server"]=>
    string(9) "Funky/1.0"
  }
  ["parentMessage%s]=>
  NULL
}
int(1)
Done
