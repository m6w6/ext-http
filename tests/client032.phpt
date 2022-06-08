--TEST--
client cookie sharing enabled
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

server("cookie.inc", function($port) {
	$client = new http\Client(null, "cookies");
	$client->configure(array("share_cookies" => true));

	$request = new http\Client\Request("GET", "http://localhost:$port");
	$client->enqueue($request);
	$client->send();
	dump_responses($client, ["counter" => 1]);

	/* requeue the previous request */
	$client->requeue($request);
	$client->send();
	dump_responses($client, ["counter" => 2]);

	for($i = 3; $i < 6; ++$i) {
		/* new requests */
		$request = new http\Client\Request("GET", "http://localhost:$port");
		$client->enqueue($request);
		$client->send();
		dump_responses($client, ["counter" => $i]);
	}

	/* requeue the previous request */
	$client->requeue($request);
	$client->send();
	dump_responses($client, ["counter" => $i]);
});

?>
===DONE===
--EXPECTF--
Test
Etag: ""
Set-Cookie: counter=1;
X-Original-Transfer-Encoding: chunked

Etag: ""
Set-Cookie: counter=2;
X-Original-Transfer-Encoding: chunked

Etag: ""
Set-Cookie: counter=3;
X-Original-Transfer-Encoding: chunked

Etag: ""
Set-Cookie: counter=4;
X-Original-Transfer-Encoding: chunked

Etag: ""
Set-Cookie: counter=5;
X-Original-Transfer-Encoding: chunked

Etag: ""
Set-Cookie: counter=6;
X-Original-Transfer-Encoding: chunked

===DONE===
