--TEST--
cookies domain
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$c = new http\Cookie("this=has a domain; domain=.example.com; ");
var_dump($c->getCookie("this"));
var_dump((string)$c);
var_dump($c->getDomain());
$o = clone $c;
$d = "sub.example.com";
$o->setDomain();
var_dump($o->getDomain());
var_dump($c->getDomain());
$o->setDomain($d);
var_dump($o->getDomain());
var_dump($c->getDomain());
var_dump($o->toString());

?>
DONE
--EXPECT--
Test
string(12) "has a domain"
string(44) "this=has%20a%20domain; domain=.example.com; "
string(12) ".example.com"
NULL
string(12) ".example.com"
string(15) "sub.example.com"
string(12) ".example.com"
string(47) "this=has%20a%20domain; domain=sub.example.com; "
DONE
