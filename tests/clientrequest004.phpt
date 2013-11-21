--TEST--
client request options
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$r = new http\Client\Request("GET", "http://localhost");
var_dump($r === $r->setOptions($o = array("redirect"=>5, "timeout"=>5)));
var_dump($o === $r->getOptions());
var_dump($r === $r->setOptions(array("timeout"=>50)));
$o["timeout"] = 50;
var_dump($o === $r->getOptions());
var_dump($r === $r->setSslOptions($o = array("verify_peer"=>false)));
var_dump($o === $r->getSslOptions());

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
