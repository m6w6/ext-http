--TEST--
HttpMessage iterator
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
?>
--FILE--
<?php
echo "-TEST\n";

$m = new HttpMessage("
GET / HTTP/1.1
HTTP/1.1 200 OK
GET /foo HTTP/1.1
HTTP/1.1 304 Not Modified
");

foreach ($m as $msg) {
	echo "==\n", $msg;
}

echo "Done\n";
?>
--EXPECTF--
%aTEST
==
HTTP/1.1 304 Not Modified
==
GET /foo HTTP/1.1
==
HTTP/1.1 200 OK
==
GET / HTTP/1.1
Done