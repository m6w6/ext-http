--TEST--
client cookie woes
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
	$client->configure(array("pipelining" => false, "share_cookies" => false));

	$request = new http\Client\Request("GET", "http://localhost:$port?r1");
	$client->enqueue($request);
	$client->send();
	dump_responses($client, ["counter" => 1]);

	$client->requeue($request);
	$client->send();
	dump_responses($client, ["counter" => 2]);
	$client->dequeue($request);

	$request = new http\Client\Request("GET", "http://localhost:$port?r2");
	$client->enqueue($request);
	$client->send();
	dump_responses($client, ["counter" => 1]);
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

===DONE===
