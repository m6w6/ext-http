--TEST--
client request gzip
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php 

include "helper/server.inc";

echo "Test\n";

server("env.inc", function($port) {
	$request = new http\Client\Request("GET", "http://localhost:$port/");
	
	$client = new http\Client;
	$client->setOptions(array("compress" => true));

	$client->enqueue($request);
	$client->send();

	echo $client->getResponse();
});
?>
===DONE===
--EXPECTF--
Test
HTTP/1.1 200 OK
Accept-Ranges: bytes
X-Request-Content-Length: 0
Vary: Accept-Encoding
Etag: "%s"
X-Original-Transfer-Encoding: chunked
X-Original-Content-Encoding: deflate
===DONE===

