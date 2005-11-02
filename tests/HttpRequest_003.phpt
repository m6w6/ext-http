--TEST--
HttpRequest SSL
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
checkcls('HttpRequest');
checkurl('arweb.info');
?>
--FILE--
<?php
echo "-TEST\n";
$r = new HttpRequest('https://ssl.arweb.info/iworks/data.txt');
$r->send();
var_dump($r->getResponseBody());
var_dump(is_object($r->getResponseMessage()));
var_dump(is_object($r->getResponseMessage()));
var_dump(is_object($r->getResponseMessage()));
?>
--EXPECTF--
%sTEST
string(10) "1234567890"
bool(true)
bool(true)
bool(true)
