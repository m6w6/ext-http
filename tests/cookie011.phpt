--TEST--
cookies extras
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";
$c = new http\Cookie("c1=v1; e0=1; e2=2; c2=v2", 0, array("e0", "e1", "e2"));
var_dump(array("c1"=>"v1", "c2"=>"v2") === $c->getCookies());
var_dump(array("e0"=>"1", "e2"=>"2") === $c->getExtras());
$c->addExtra("e1", 1);
$c->setExtra("e0");
$c->setExtra("e3", 123);
var_dump("123" === $c->getExtra("e3"));
$c->setExtra("e3");
var_dump(array("e2"=>"2", "e1"=>"1") === $c->getExtras());
var_dump("c1=v1; c2=v2; e2=2; e1=1; " === $c->toString());
$c->addExtras(array("e3"=>3, "e4"=>4));
var_dump(array("e2"=>"2", "e1"=>"1", "e3"=>"3", "e4"=>"4") === $c->getExtras());
var_dump("c1=v1; c2=v2; e2=2; e1=1; e3=3; e4=4; " === $c->toString());
$c->setExtras(array("e"=>"x"));
var_dump(array("e"=>"x") === $c->getExtras());
var_dump("c1=v1; c2=v2; e=x; " === $c->toString());
$c->setExtras();
var_dump(array() === $c->getExtras());
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
