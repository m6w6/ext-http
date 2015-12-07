--TEST--
client stream 128M
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";
include "helper/dump.inc";

echo "Test\n";

server("proxy.inc", function($port) {
	$client = new http\Client;
	$request = new http\Client\Request("PUT", "http://localhost:$port");
	$request->setContentType("application/octet-stream");
	for ($i = 0, $data = str_repeat("a",1024); $i < 128*1024; ++$i) {
		$request->getBody()->append($data);
	}
	$request->setOptions(array("timeout" => 10, "expect_100_timeout" => 0));
	$client->enqueue($request);
	$client->send();
	dump_headers(null, $client->getResponse()->getHeaders());
});

?>
===DONE===
--EXPECTF--
Test
Accept-Ranges: bytes
Content-Length: %d
Etag: "%x"
Last-Modified: %s
X-Original-Transfer-Encoding: chunked
X-Request-Content-Length: 134217728

===DONE===
