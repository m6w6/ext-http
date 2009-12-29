--TEST--
HttpRequest SSL
--SKIPIF--
<?php
include 'skip.inc';
checkmin("5.2.5");
checkurl('arweb.info');
skipif(!http_support(HTTP_SUPPORT_SSLREQUESTS), 'need ssl-request support')
?>
--FILE--
<?php
echo "-TEST\n";
$o = array('redirect' => '3', 'ssl' => array('version' => '3', 'verifyhost' => '1'));
$r = new HttpRequest('https://ssl.irmler.at/iworks/data.txt');
$r->setOptions($o);
$r->send();
var_dump($r->getResponseBody());
var_dump(is_object($r->getResponseMessage()));
var_dump(is_object($r->getResponseMessage()));
var_dump(is_object($r->getResponseMessage()));
var_dump($o);
$r->setOptions($o);
$r->send();
var_dump($o);
?>
--EXPECTF--
%aTEST
string(10) "1234567890"
bool(true)
bool(true)
bool(true)
array(2) {
  ["redirect"]=>
  string(1) "3"
  ["ssl"]=>
  array(2) {
    ["version"]=>
    string(1) "3"
    ["verifyhost"]=>
    string(1) "1"
  }
}
array(2) {
  ["redirect"]=>
  string(1) "3"
  ["ssl"]=>
  array(2) {
    ["version"]=>
    string(1) "3"
    ["verifyhost"]=>
    string(1) "1"
  }
}

