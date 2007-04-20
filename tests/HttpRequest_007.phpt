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

$r = new HttpRequest('http://dev.iworks.at/ext-http/.print_put.php5', HTTP_METH_PUT);
$r->recordHistory = true;
$r->addHeaders(array('content-type' => 'text/plain'));
$r->setPutFile(__FILE__);
$r->send();
var_dump($r->getHistory()->toString(true));
echo "Done\n";
?>
--EXPECTF--
%sTEST
string(%d) "PUT /ext-http/.print_put.php5 HTTP/1.1
User-Agent: PECL::HTTP/%s
Host: dev.iworks.at
Accept: */*
Content-Type: text/plain
Content-Length: %d
Expect: 100-continue

<?php
echo "-TEST\n";

$r = new HttpRequest('http://dev.iworks.at/ext-http/.print_put.php5', HTTP_METH_PUT);
$r->recordHistory = true;
$r->addHeaders(array('content-type' => 'text/plain'));
$r->setPutFile(__FILE__);
$r->send();
var_dump($r->getHistory()->toString(true));
echo "Done\n";
?>

HTTP/1.1 100 Continue
HTTP/1.1 200 OK
Date: %s
Server: %s
X-Powered-By: %s
Vary: Accept-Encoding
Content-Length: %d
Content-Type: text/html

<?php
echo "-TEST\n";

$r = new HttpRequest('http://dev.iworks.at/ext-http/.print_put.php5', HTTP_METH_PUT);
$r->recordHistory = true;
$r->addHeaders(array('content-type' => 'text/plain'));
$r->setPutFile(__FILE__);
$r->send();
var_dump($r->getHistory()->toString(true));
echo "Done\n";
?>

"
Done
