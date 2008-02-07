--TEST--
HttpMessage::prepend()
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
?>
--FILE--
<?php
echo "-TEST\n";

$m1 = new HttpMessage("
GET / HTTP/1.1
Host: example.com
Accept: */*
Connection: close
HTTP/1.1 200 ok
Server: Funky/1.0
Content-Type: text/plain
Content-Length: 3

Hi!"
);

$m2 = new HttpMessage("
GET http://example.com/ HTTP/1.0
HTTP/1.1 200 ok
Server: Funky/2.0
Content-Type: text/html
Content-Length: 9

Hi there!"
);

$m1->prepend($m2);
$m2 = NULL;
echo $m1->toString(true);

$m1->prepend($m1->detach(), false);
echo $m1->toString(true);

echo "Done\n";
?>
--EXPECTF--
%aTEST
GET http://example.com/ HTTP/1.0
HTTP/1.1 200 ok
Server: Funky/2.0
Content-Type: text/html
Content-Length: 9

Hi there!
GET / HTTP/1.1
Host: example.com
Accept: */*
Connection: close
HTTP/1.1 200 ok
Server: Funky/1.0
Content-Type: text/plain
Content-Length: 3

Hi!
GET http://example.com/ HTTP/1.0
HTTP/1.1 200 ok
Server: Funky/2.0
Content-Type: text/html
Content-Length: 9

Hi there!
GET / HTTP/1.1
Host: example.com
Accept: */*
Connection: close
HTTP/1.1 200 ok
Server: Funky/1.0
Content-Type: text/plain
Content-Length: 3

Hi!
HTTP/1.1 200 ok
Server: Funky/1.0
Content-Type: text/plain
Content-Length: 3

Hi!
Done