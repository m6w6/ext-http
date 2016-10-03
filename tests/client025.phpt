--TEST--
client seek
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/dump.inc";
include "helper/server.inc";

echo "Test\n";

server("proxy.inc", function($port) {
	$client = new http\Client;
	$request = new http\Client\Request("PUT", "http://localhost:$port");
	$request->setOptions(array("resume" => 1, "expect_100_timeout" => 0));
	$request->getBody()->append("123");
	dump_message(null, $client->enqueue($request)->send()->getResponse());
});
// Content-length is 2 instead of 3 in older libcurls
?>
===DONE===
--EXPECTF--
Test
HTTP/1.1 200 OK
Accept-Ranges: bytes
Content-Length: %d
Etag: "%x"
X-Original-Transfer-Encoding: chunked
X-Request-Content-Length: 2

PUT / HTTP/1.1
Accept: */*
Content-Length: %d
Content-Range: bytes 1-2/3
Expect: 100-continue
Host: localhost:%d
User-Agent: %s
X-Original-Content-Length: %d

23===DONE===
