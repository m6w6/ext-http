--TEST--
client response callback + requeue
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
}

server("proxy.inc", function($port) {
	$request = new http\Client\Request("GET", "http://localhost:$port");
	
	foreach (http\Client::getAvailableDrivers() as $driver) {
		$client = new http\Client($driver);
		for ($i=0; $i < 2; ++ $i) {
			$client->requeue($request, "response");
			$client->send();
		}
	}
});

?>
Done
--EXPECTREGEX--
Test
(?:R
R
)+Done
