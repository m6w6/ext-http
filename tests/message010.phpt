--TEST--
message body
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$m = new http\Message;
$body = new http\Message\Body;
$body->append("foo");
$m->addBody($body);
$body = new http\Message\Body;
$body->append("bar");
$m->addBody($body);
var_dump("foobar" === (string) $m->getBody());

?>
Done
--EXPECT--
Test
bool(true)
Done
