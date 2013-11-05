--TEST--
client ssl
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

$client = new http\Client;

$client->setSslOptions(array("verify_peer" => true));
$client->addSslOptions(array("verify_host" => 2));
var_dump(
	array(
		"verify_peer" => true,
		"verify_host" => 2,
	) === $client->getSslOptions()
);

$client->enqueue($req = new http\Client\Request("GET", "https://twitter.com/"));
$client->send();

$ti = (array) $client->getTransferInfo($req);
var_dump(array_key_exists("ssl_engines", $ti));
var_dump(0 < count($ti["ssl_engines"]));
?>
Done
--EXPECTF--
Test
bool(true)
bool(true)
bool(true)
Done
