--TEST--
Bug #66388 (Crash on POST with Content-Length:0 and untouched body)
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
skip_online_test();
?>
--FILE--
<?php

use http\Client,
	http\Client\Request;

include "helper/server.inc";

echo "Test\n";

server("proxy.inc", function($port) {
	$client = new Client();
	$request = new Request(
		'POST',
		"http://localhost:$port/",
		array(
			'Content-Length' => 0
		)
	);
	$client->setOptions(["timeout" => 30]);
	$client->enqueue($request);
	echo $client->send()->getResponse()->getResponseCode();
});

?>

===DONE===
--EXPECTF--
Test
200
===DONE===
