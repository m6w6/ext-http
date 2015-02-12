--TEST--
client response callback
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
	foreach (http\Client::getAvailableDrivers() as $driver) {
		$client = new http\Client($driver);
		$client->enqueue(new http\Client\Request("GET", "http://localhost:$port"), function($response) {
			echo "R\n";
			if (!($response instanceof http\Client\Response)) {
				var_dump($response);
			}
		});
		$client->send();
	}
});

?>
Done
--EXPECTREGEX--
Test
(?:R
)+Done
