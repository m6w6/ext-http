--TEST--
version parse error
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
$m = new http\Message;
$m->setHttpVersion("1-1");
$m->setHttpVersion("one.one");
?>
--EXPECTF--
Notice: http\Message::setHttpVersion(): Non-standard version separator '-' in HTTP protocol version '1-1' in %s

Warning: http\Message::setHttpVersion(): Could not parse HTTP protocol version 'one.one' in %s

