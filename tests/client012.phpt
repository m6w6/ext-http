--TEST--
client ssl
--SKIPIF--
<?php 
include "skipif.inc";
skip_online_test();
skip_client_test();
?>
--FILE--
<?php 
echo "Test\n";

$client = new http\Client;

$client->setSslOptions(array("verifypeer" => true));
$client->addSslOptions(array("verifyhost" => 2));
var_dump(
	array(
		"verifypeer" => true,
		"verifyhost" => 2,
	) === $client->getSslOptions()
);

$client->enqueue($req = new http\Client\Request("GET", "https://twitter.com/"));
$client->send();

$ti = (array) $client->getTransferInfo($req);
var_dump(array_key_exists("ssl_engines", $ti));
var_dump(0 < count($ti["ssl_engines"]) || $ti["tls_session"]["backend"] != "openssl");
?>
Done
--EXPECTF--
Test
bool(true)
bool(true)
bool(true)
Done
