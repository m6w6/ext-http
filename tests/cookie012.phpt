--TEST--
cookies cookies
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";
$c = new http\Cookie("e0=1; c1=v1; e2=2; c2=v2", 0, array("c0", "c1", "c2"));
var_dump(array("c1"=>"v1", "c2"=>"v2") === $c->getExtras());
var_dump(array("e0"=>"1", "e2"=>"2") === $c->getCookies());
$c->addCookie("e1", 1);
$c->setCookie("e0");
$c->setCookie("e3", 123);
var_dump("123" === $c->getCookie("e3"));
$c->setCookie("e3");
var_dump(array("e2"=>"2", "e1"=>"1") === $c->getCookies());
var_dump("e2=2; e1=1; c1=v1; c2=v2; " === $c->toString());
$c->addCookies(array("e3"=>3, "e4"=>4));
var_dump(array("e2"=>"2", "e1"=>"1", "e3"=>"3", "e4"=>"4") === $c->getCookies());
var_dump("e2=2; e1=1; e3=3; e4=4; c1=v1; c2=v2; " === $c->toString());
$c->setCookies(array("e"=>"x"));
var_dump(array("e"=>"x") === $c->getCookies());
var_dump("e=x; c1=v1; c2=v2; " === $c->toString());
$c->setCookies();
var_dump(array() === $c->getCookies());
var_dump("c1=v1; c2=v2; " === $c->toString());

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
DONE
