--TEST--
HttpRequestDataShare global
--SKIPIF--
<?php
include "skip.inc";
checkurl("www.google.com");
checkcls("HttpRequestDataShare");
?>
--FILE--
<?php
echo "-TEST\n";

$s = HttpRequestDataShare::singleton(true);
$s->cookie = true;
var_dump($s);

$r1 = new HttpRequest("http://www.google.com/");
$r2 = new HttpRequest("http://www.google.com/");

$r1->enableCookies();
$r2->enableCookies();

$s->attach($r1);
$s->attach($r2);

$r1->send();
$r2->send();

$s->reset();

if (current($r1->getResponseCookies())->cookies["PREF"] !== HttpUtil::parseCookie($r2->getRequestMessage()->getHeader("Cookie"))->cookies["PREF"]) {
	var_dump(
		current($r1->getResponseCookies())->cookies["PREF"],
		HttpUtil::parseCookie($r2->getRequestMessage()->getHeader("Cookie"))->cookies["PREF"]
	);
}

echo "Done\n";
?>
--EXPECTF--
%sTEST
object(HttpRequestDataShare)#1 (4) {
  ["cookie"]=>
  bool(true)
  ["dns"]=>
  bool(true)
  ["ssl"]=>
  bool(false)
  ["connect"]=>
  bool(false)
}
Done
