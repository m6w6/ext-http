--TEST--
HttpRequest PUT
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpRequest');
?>
--FILE--
<?php
echo "-TEST\n";

$r = new HttpRequest('http://dev.iworks.at/.print_request.php', HTTP_METH_PUT);
$r->recordHistory = true;
$r->addHeaders(array('content-type' => 'text/plain'));
$r->setPutFile(__FILE__);
$r->send();
var_dump($r->getRawRequestMessage());
echo "Done\n";
?>
--EXPECTF--
%sTEST
string(%d) "PUT /.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
content-type: text/plain
Content-Length: 278
Expect: 100-continue

<?php
echo "-TEST\n";

$r = new HttpRequest('http://dev.iworks.at/.print_request.php', HTTP_METH_PUT);
$r->recordHistory = true;
$r->addHeaders(array('content-type' => 'text/plain'));
$r->setPutFile(__FILE__);
$r->send();
var_dump($r->getRawRequestMessage());
echo "Done\n";
?>
"
Done

