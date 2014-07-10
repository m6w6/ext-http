--TEST--
cookies path
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$c = new http\Cookie("this=has a path; path=/down; ");
var_dump($c->getCookie("this"));
var_dump((string)$c);
var_dump($c->getPath());
$o = clone $c;
$p = "/up";
$o->setPath();
var_dump($o->getPath());
var_dump($c->getPath());
$o->setPath($p);
var_dump($o->getPath());
var_dump($c->getPath());
var_dump($o->toString());

?>
DONE
--EXPECT--
Test
string(10) "has a path"
string(33) "this=has%20a%20path; path=/down; "
string(5) "/down"
NULL
string(5) "/down"
string(3) "/up"
string(5) "/down"
string(31) "this=has%20a%20path; path=/up; "
DONE
