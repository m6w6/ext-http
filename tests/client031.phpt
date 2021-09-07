--TEST--
client cookie sharing disabled
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
	$client->configure(array("share_cookies" => false));

	$request = new http\Client\Request("GET", "http://localhost:$port");
	$client->enqueue($request);
	$client->send();
	dump_responses($client, ["counter" => 1]);

	/* requeue the previous request */
	$client->requeue($request);
	$client->send();
	dump_responses($client, ["counter" => 2]);

	for($i = 0; $i < 3; ++$i) {
		/* new requests */
		$request = new http\Client\Request("GET", "http://localhost:$port");
		$client->enqueue($request);
		$client->send();
		dump_responses($client, ["counter" => 1]);
	}

	/* requeue the previous request */
	$client->requeue($request);
	$client->send();
	dump_responses($client, ["counter" => 2]);
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
Set-Cookie: counter=1;
X-Original-Transfer-Encoding: chunked

Etag: ""
Set-Cookie: counter=1;
X-Original-Transfer-Encoding: chunked

Etag: ""
Set-Cookie: counter=1;
X-Original-Transfer-Encoding: chunked

Etag: ""
Set-Cookie: counter=2;
X-Original-Transfer-Encoding: chunked

===DONE===
