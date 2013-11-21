--TEST--
client request query
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$r = new http\Client\Request("GET", "http://localhost/");
var_dump(null === $r->getQuery());
var_dump($r === $r->setQuery($q = "foo=bar"));
var_dump($q === $r->getQuery());
var_dump($r === $r->addQuery("a[]=1&a[]=2"));
var_dump("foo=bar&a%5B0%5D=1&a%5B1%5D=2" === $r->getQuery());
var_dump(null === $r->setQuery(null)->getQuery());

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Done
