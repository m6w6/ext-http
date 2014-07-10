--TEST--
cookies max-age
--SKIPIF--
<?php
include "skipif.inc";
?>
--INI--
date.timezone=UTC
--FILE--
<?php
echo "Test\n";

$c = new http\Cookie("this=max-age; max-age=12345");
var_dump($c->getCookie("this"));
var_dump($c->getMaxAge());
$o = clone $c;
$t = 54321;
$o->setMaxAge();
var_dump($o->getMaxAge());
var_dump(-1 != $c->getMaxAge());
$o->setMaxAge($t);
var_dump($o->getMaxAge());
var_dump($t != $c->getMaxAge());
var_dump($o->toString());

?>
DONE
--EXPECT--
Test
string(7) "max-age"
int(12345)
int(-1)
bool(true)
int(54321)
bool(true)
string(29) "this=max-age; max-age=54321; "
DONE
