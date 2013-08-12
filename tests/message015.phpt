--TEST--
message errors
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

$m = new http\Message;
$m->setRequestUrl("/foo");
$m->setType(http\Message::TYPE_REQUEST);
$m->setRequestUrl("");

$m = new http\Message;
try {
	$m->getParentMessage();
	die("unreached");
} catch (http\Exception $e) {
	var_dump($e->getMessage());
}

$m = new http\Message("HTTP/1.1 200\r\nHTTP/1.1 201");
try {
	$m->prepend($m->getParentMessage());
	die("unreached");
} catch (http\Exception $e) {
	var_dump($e->getMessage());
}

?>
Done
--EXPECTF--
Test

Notice: http\Message::setRequestUrl(): HttpMessage is not of type REQUEST in %s on line %d

Warning: http\Message::setRequestUrl(): Cannot set HttpMessage::requestUrl to an empty string in %s on line %d
string(42) "HttpMessage does not have a parent message"
string(62) "Cannot prepend a message located within the same message chain"
Done
