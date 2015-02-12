--TEST--
client response callback + dequeue
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";

function response($response) {
	echo "R\n";
	if (!($response instanceof http\Client\Response)) {
		var_dump($response);
	}
	
	/* automatically dequeue */
	return true;
}

server("proxy.inc", function($port) {
	$request = new http\Client\Request("GET", "http://localhost:$port");
	
	foreach (http\Client::getAvailableDrivers() as $driver) {
		$client = new http\Client($driver);
		for ($i=0; $i < 2; ++ $i) {
			$client->enqueue($request, "response");
			$client->send();
			try {
				$client->dequeue($request);
			} catch (Exception $e) {
				echo $e->getMessage(),"\n";
			}
		}
	}
});

?>
Done
--EXPECTREGEX--
Test
(?:(?:R
Failed to dequeue request; request not in queue
)+)+Done
