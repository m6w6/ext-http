--TEST--
version parse error
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
error_reporting(E_ALL);
$m = new http\Message;
try {
	$m->setHttpVersion("1-1");
	$m->setHttpVersion("one.one");
} catch (http\Exception $e) {
	echo $e->getMessage(),"\n";
}
?>
--EXPECTF--
Notice: http\Message::setHttpVersion(): Non-standard version separator '-' in HTTP protocol version '1-1' in %s
http\Message::setHttpVersion(): Could not parse HTTP protocol version 'one.one'

