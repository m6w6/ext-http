--TEST--
client history
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
	$body = new http\Message\Body;
	$body->append("foobar");

	$request = new http\Client\Request;
	$request->setBody($body);
	$request->setRequestMethod("POST");
	$request->setRequestUrl("http://localhost:$port");

	$client = new http\Client;
	$client->recordHistory = true;

	$client->enqueue($request)->send();
	echo $client->getHistory()->toString(true);

	$client->requeue($request)->send();
	echo $client->getHistory()->toString(true);
});
?>
Done
--EXPECTF--
Test
POST http://localhost:%d/ HTTP/1.1
Content-Length: 6

foobar
HTTP/1.1 200 OK
Accept-Ranges: bytes
X-Request-Content-Length: 6
X-Original-Transfer-Encoding: chunked
Content-Length: 19

string(6) "foobar"

POST http://localhost:%d/ HTTP/1.1
Content-Length: 6

foobar
HTTP/1.1 200 OK
Accept-Ranges: bytes
X-Request-Content-Length: 6
X-Original-Transfer-Encoding: chunked
Content-Length: 19

string(6) "foobar"

POST http://localhost:%d/ HTTP/1.1
Content-Length: 6

foobar
HTTP/1.1 200 OK
Accept-Ranges: bytes
X-Request-Content-Length: 6
X-Original-Transfer-Encoding: chunked
Content-Length: 19

string(6) "foobar"

Done
