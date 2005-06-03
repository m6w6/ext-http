--TEST--
HttpRequest SSL
--SKIPIF--
<?php
include 'skip.inc';
(5 > (int) PHP_VERSION) and die('skip PHP5 is required for Http classes');
?>
--FILE--
<?php
$r = new HttpRequest('https://ssl.arweb.info/iworks/data.txt');
$r->send();
var_dump($r->getResponseBody());
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

string(10) "1234567890"
