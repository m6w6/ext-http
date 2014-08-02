--TEST--
cookies flags
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$c = new http\Cookie("icanhas=flags; secure; httpOnly");
var_dump(http\Cookie::SECURE === ($c->getFlags() & http\Cookie::SECURE));
var_dump(http\Cookie::HTTPONLY === ($c->getFlags() & http\Cookie::HTTPONLY));
$c->setFlags($c->getFlags() ^ http\Cookie::SECURE);
var_dump(!($c->getFlags() & http\Cookie::SECURE));
var_dump(http\Cookie::HTTPONLY === ($c->getFlags() & http\Cookie::HTTPONLY));
$c->setFlags($c->getFlags() ^ http\Cookie::HTTPONLY);
var_dump(!($c->getFlags() & http\Cookie::SECURE));
var_dump(!($c->getFlags() & http\Cookie::HTTPONLY));
var_dump("icanhas=flags; " === $c->toString());
$c->setFlags(http\Cookie::SECURE|http\Cookie::HTTPONLY);
var_dump("icanhas=flags; secure; httpOnly; " === $c->toString());
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
DONE
