--TEST--
client request gzip
--SKIPIF--
<?php 
include "skipif.inc";
skip_online_test();
skip_client_test();
?>
--FILE--
<?php 

echo "Test\n";

$client = new http\Client;
$client->setOptions(array("compress" => true));

$client->enqueue(new http\Client\Request("GET", "http://dev.iworks.at/ext-http/.print_request.php"));
$client->send();

echo $client->getResponse();

?>
===DONE===
--EXPECTF--
Test
HTTP/1.1 200 OK
Vary: Accept-Encoding
Content-Type: text/html
Date: %s
Server: %s
X-Original-Transfer-Encoding: chunked
X-Original-Content-Encoding: gzip
===DONE===