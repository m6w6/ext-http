--TEST--
HttpMessage::detach()
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
Host: example.com
Accept: */*
Connection: close
HTTP/1.1 200 ok
Server: Funky/1.0
Content-Type: text/plain
Content-Length: 3

Hi!"
);

$d = $m->detach();
$d->addHeaders(array('Server'=>'Funky/2.0'));
var_dump($d->getHeaders() == $m->getHeaders());
var_dump($d->getBody());

echo "Done\n";
?>
--EXPECTF--
%aTEST
bool(false)
string(3) "Hi!"
Done