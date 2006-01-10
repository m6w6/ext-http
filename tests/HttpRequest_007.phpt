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

$r = new HttpRequest('http://dev.iworks.at/.print_put.php5', HTTP_METH_PUT);
$r->recordHistory = true;
$r->addHeaders(array('content-type' => 'text/plain'));
$r->setPutFile(__FILE__);
$r->send();
var_dump($r->getHistory()->toString(true));
echo "Done\n";
?>
--EXPECTF--
%sTEST
string(%d) "HTTP/1.1 200 OK
Date: %s
Server: Apache/%s
X-Powered-By: PHP/5%s
Vary: Accept-Encoding
Content-Type: text/html
X-Original-Transfer-Encoding: chunked
Content-Length: 281

<?php
echo "-TEST\n";

$r = new HttpRequest('http://dev.iworks.at/.print_put.php5', HTTP_METH_PUT);
$r->recordHistory = true;
$r->addHeaders(array('content-type' => 'text/plain'));
$r->setPutFile(__FILE__);
$r->send();
var_dump($r->getHistory()->toString(true));
echo "Done\n";
?>

PUT /.print_put.php5 HTTP/1.1
User-Agent: PECL::HTTP/%s
Host: dev.iworks.at
Accept: */*
Content-Type: text/plain
Content-Length: 281
Expect: 100-continue

<?php
echo "-TEST\n";

$r = new HttpRequest('http://dev.iworks.at/.print_put.php5', HTTP_METH_PUT);
$r->recordHistory = true;
$r->addHeaders(array('content-type' => 'text/plain'));
$r->setPutFile(__FILE__);
$r->send();
var_dump($r->getHistory()->toString(true));
echo "Done\n";
?>

"
Done

