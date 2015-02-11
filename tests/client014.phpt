--TEST--
reset content length when resetting body
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php 

echo "Test\n";

$client = new http\Client;
$request = new http\Client\Request("PUT", "put.php");
$request->setBody(new http\Message\Body(fopen(__FILE__, "r")));
$client->enqueue($request);
var_dump($request->getHeader("Content-Length"));
$request->setBody(new http\Message\Body);
$client->requeue($request);
var_dump($request->getHeader("Content-Length"));
?>
===DONE===
--EXPECTF--
Test
int(379)
bool(false)
===DONE===
