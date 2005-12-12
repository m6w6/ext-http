--TEST--
HttpMessage
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
	"HTTP/1.1 206\r\n".
	"Content-Range: bytes=2-3\r\n".
	"Transfer-Encoding: chunked\r\n".
	"\r\n".
	"01\r\n".
	"X\r\n".
	"00"
);

var_dump($m->getBody());
var_dump(HttpMessage::fromString($m->toString(true))->toString(true));
do {
	var_dump($m->toString());
} while ($m = $m->getParentMessage());

echo "Done\n";
?>
--EXPECTF--
%sTEST
string(1) "X"
string(174) "HTTP/1.1 301
Location: /anywhere
HTTP/1.1 302
Location: /somewhere
HTTP/1.1 206
Content-Range: bytes=2-3
X-Original-Transfer-Encoding: chunked
Content-Length: 1

X
"
string(103) "HTTP/1.1 206
Content-Range: bytes=2-3
X-Original-Transfer-Encoding: chunked
Content-Length: 1

X
"
string(36) "HTTP/1.1 302
Location: /somewhere
"
string(35) "HTTP/1.1 301
Location: /anywhere
"
Done
