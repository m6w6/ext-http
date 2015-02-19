--TEST--
client seek
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

include "helper/server.inc";

echo "Test\n";

server("proxy.inc", function($port) {
	$client = new http\Client;
	$request = new http\Client\Request("PUT", "http://localhost:$port");
	$request->setOptions(array("resume" => 1, "expect_100_timeout" => 0));
	$request->getBody()->append("123");
	echo $client->enqueue($request)->send()->getResponse();
});

?>
===DONE===
--EXPECTF--
Test
HTTP/1.1 200 OK
Accept-Ranges: bytes
Etag: "%x"
X-Original-Transfer-Encoding: chunked
Content-Length: %d

PUT / HTTP/1.1
Content-Range: bytes 1-2/3
User-Agent: %s
Host: localhost:%d
Accept: */*
Content-Length: 3
Expect: 100-continue
X-Original-Content-Length: 3

23===DONE===
