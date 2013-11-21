--TEST--
client reset
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$request = new http\Client\Request("GET", "http://www.example.org");

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	$client->enqueue($request)->send();
	if (!($client->getResponse($request) instanceof http\Client\Response)) {
		var_dump($client);
	}
	try {
		$client->enqueue($request);
	} catch (Exception $e) {
		echo $e->getMessage(),"\n";
	}
	$client->reset();
	if (($response = $client->getResponse())) {
		var_dump($response);
	}
	$client->enqueue($request);
}
?>
Done
--EXPECTREGEX--
Test
(?:Failed to enqueue request; request already in queue
)+Done
