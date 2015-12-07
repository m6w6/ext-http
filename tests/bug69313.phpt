--TEST--
Bug #69313 (http\Client doesn't send GET body)
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

server("proxy.inc", function($port, $stdin, $stdout, $stderr) {
	$request = new http\Client\Request("GET", "http://localhost:$port/");
	$request->setHeader("Content-Type", "text/plain");
	$request->getBody()->append("foo");
	$client = new http\Client();
	$client->enqueue($request);
	$client->send();
	dump_message(null, $client->getResponse());
});

?>

Done
--EXPECTF--
Test
HTTP/1.1 200 OK
Accept-Ranges: bytes
Content-Length: %d
Etag: "%s"
X-Original-Transfer-Encoding: chunked
X-Request-Content-Length: 3

GET / HTTP/1.1
Accept: */*
Content-Length: 3
Content-Type: text/plain
Host: localhost:%d
User-Agent: %s
X-Original-Content-Length: 3

foo
Done
