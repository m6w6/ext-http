--TEST--
HttpRequestDataShare global
--SKIPIF--
<?php
include "skip.inc";
checkcls("HttpRequestDataShare");
?>
--INI--
http.request.datashare.cookie = 1
--FILE--
<?php
echo "-TEST\n";

$s = HttpRequestDataShare::singleton(true);

$r1 = new HttpRequest("http://www.google.com/");
$r2 = new HttpRequest("http://www.google.com/");

$r1->enableCookies();
$r2->enableCookies();

$s->attach($r1);
$s->attach($r2);

$r1->send();
$r2->send();

$s->reset();

var_dump(current($r1->getResponseCookies())->cookies["PREF"] === HttpUtil::parseCookie($r2->getRequestMessage()->getHeader("Cookie"))->cookies["PREF"]);

echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
Done
