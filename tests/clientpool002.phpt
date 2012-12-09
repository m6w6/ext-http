--TEST--
pool iteration
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

use http\Client;
use http\Curl;

class UserPool extends Client\Pool\AbstractPool {
}
class UserClient extends Client\AbstractClient {
	function send($request = null) {
	}
}

echo "CURL\n";

$client = new Curl\Client();
$pool = new Curl\Client\Pool();

echo "Iterator: ";
foreach ($pool as $c) {
	if ($c !== $client) {
		echo ".";
	}
}
echo "\n";
echo "Attached: ";
foreach ($pool->getAttached() as $c) {
	if ($c !== $client) {
		echo ".";
	}
}
echo "\n";
echo "Finished: ";
foreach ($pool->getFinished() as $c) {
	echo ".";
}
echo "\n";

echo "USER\n";

$client = new UserClient();
$pool = new UserPool();

echo "Iterator: ";
foreach ($pool as $c) {
	if ($c !== $client) {
		echo ".";
	}
}
echo "\n";
echo "Attached: ";
foreach ($pool->getAttached() as $c) {
	if ($c !== $client) {
		echo ".";
	}
}
echo "\n";
echo "Finished: ";
foreach ($pool->getFinished() as $c) {
	echo ".";
}
echo "\n";
?>
Done
--EXPECT--
Test
CURL
Iterator: 
Attached: 
Finished: 
USER
Iterator: 
Attached: 
Finished: 
Done