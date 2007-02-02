--TEST--
HttpRequest custom request method
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpRequest');
?>
--FILE--
<?php
echo "-TEST\n";

HttpRequest::methodRegister('foobar');
$r = new HttpRequest('http://dev.iworks.at/.print_request.php', HttpRequest::METH_FOOBAR);
$r->setContentType('text/plain');
$r->setBody('Yep, this is FOOBAR!');
var_dump($r->send()->getResponseCode());
var_dump($r->getRawRequestMessage());

echo "Done\n";
?>
--EXPECTF--
%sTEST
int(200)
string(%d) "FOOBAR /.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
Content-Type: text/plain
Content-Length: 20

Yep, this is FOOBAR!"
Done
