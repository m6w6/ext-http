--TEST--
client response transfer info
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";

server("proxy.inc", function($port) {
	$request = new http\Client\Request("GET", "http://localhost:$port");
	
	foreach (http\Client::getAvailableDrivers() as $driver) {
		$client = new http\Client($driver);
		$response = $client->enqueue($request)->send()->getResponse();
		var_dump($response->getTransferInfo("response_code"));
		var_dump(count((array)$response->getTransferInfo()));
	}
});
?>
Done
--EXPECTREGEX--
Test
(?:int\([1-5]\d\d\)
int\(\d\d\)
)+Done
