--TEST--
client once & wait with events
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
	$request = new http\Client\Request("GET", "http://localhost:$port/");
	
	foreach (http\Client::getAvailableDrivers() as $driver) {
		$client = new http\Client($driver);
		$client->configure(array("use_eventloop" => true));
		$client->enqueue($request);
	
		while ($client->once()) {
			$client->wait(.1);
		}
	
		if (!$client->getResponse()) {
			var_dump($client);
		}
	}
});
?>
Done
--EXPECT--
Test
Done
