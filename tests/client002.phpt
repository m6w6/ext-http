--TEST--
client observer
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

class Observer implements SplObserver
{
	function update(SplSubject $client, http\Client\Request $request = null, StdClass $progress = null) {
		echo "P";
		if ($client->getProgressInfo($request) != $progress) {
			var_dump($progress);
		}
	}
}

$observer = new Observer;
$request = new http\Client\Request("GET", "http://www.example.org/");

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	$client->attach($observer);
	$client->enqueue($request);
	$client->send();
}

?>

Done
--EXPECTREGEX--
Test
P+
Done
