--TEST--
client response callback + requeue
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

function response($response) {
	echo "R\n";
	if (!($response instanceof http\Client\Response)) {
		var_dump($response);
	}
}

$request = new http\Client\Request("GET", "http://www.example.org");

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	for ($i=0; $i < 2; ++ $i) {
		$client->requeue($request, "response");
		$client->send();
	}
}

?>
Done
--EXPECTREGEX--
Test
(?:R
R
)+Done
