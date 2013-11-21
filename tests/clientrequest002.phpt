--TEST--
client request content type
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$r = new http\Client\Request("POST", "http://localhost/");
var_dump($r === $r->setContentType($ct = "text/plain; charset=utf-8"));
var_dump($ct === $r->getContentType());

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
Done
