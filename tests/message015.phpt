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
try {
	$m->setRequestUrl("/foo");
} catch (http\Exception $e) {
	echo $e->getMessage(),"\n";
}
$m->setType(http\Message::TYPE_REQUEST);
try {
	$m->setRequestUrl("");
} catch (http\Exception $e) {
	echo $e->getMessage(),"\n";
}

$m = new http\Message;
try {
	$m->getParentMessage();
	die("unreached");
} catch (http\Exception $e) {
	echo $e->getMessage(),"\n";
}

$m = new http\Message("HTTP/1.1 200\r\nHTTP/1.1 201");
try {
	$m->prepend($m->getParentMessage());
	die("unreached");
} catch (http\Exception $e) {
	echo $e->getMessage(),"\n";
}

?>
Done
--EXPECTF--
Test
http\Message is not of type request
Cannot set http\Message's request url to an empty string
http\Message has not parent message
Cannot prepend a message located within the same message chain
Done
