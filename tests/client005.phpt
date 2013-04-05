--TEST--
client response callback
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	$client->enqueue(new http\Client\Request("GET", "http://www.example.org"), function($response) {
		echo "R\n";
		if (!($response instanceof http\Client\Response)) {
			var_dump($response);
		}
	});
	$client->send();
}

?>
Done
--EXPECTREGEX--
Test
(?:R
)+Done
