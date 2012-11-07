--TEST--
message cloning
--SKIPIF--
<?php include "skipif.inc";
--FILE--
<?php

$msg = new http\Message("
HTTP/1.1 200 Ok
String: foobar
");

$cpy = clone $msg;

$cpy->setType(http\Message::TYPE_REQUEST);
$cpy->setHeaders(array("Numbers" => array(1,2,3,4.5)));

echo $msg;
echo "\n===\n";
echo $cpy;

?>
DONE
--EXPECTF--
HTTP/1.1 200 Ok
String: foobar

===
UNKNOWN / HTTP/1.1
Numbers: 1, 2, 3, 4.5
DONE
