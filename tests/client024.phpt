--TEST--
client deprecated methods
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

echo "Test\n";

$client = new http\Client;
$client->enableEvents(false);
$client->enablePipelining(false);

?>
===DONE===
--EXPECTF--
Test

Deprecated: Method http\Client::enableEvents() is deprecated in %sclient024.php on line %d

Deprecated: Method http\Client::enablePipelining() is deprecated in %sclient024.php on line %d
===DONE===
