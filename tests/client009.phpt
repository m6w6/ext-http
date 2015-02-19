--TEST--
client static cookies
--SKIPIF--
<?php
include "skipif.inc";
skip_online_test();
skip_client_test();
?>
--FILE--
<?php
echo "Test\n";

$request = new http\Client\Request("GET", "http://dev.iworks.at/ext-http/.print_request.php");

function x($a) {
	$r[key($a)]=end($a);
	$r[key($a)]=reset($a);
	return $r;
}
foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	$client->setCookies(array("test" => "bar"));
	$client->addCookies(array("foo" => "test"));
	$client->enqueue($request);
	$client->send();
	var_dump($client->getResponse()->getBody()->toString());
	$request->setOptions(array("cookies" => x($client->getCookies())));
	$client->requeue($request);
	$client->send();
	var_dump($client->getResponse()->getBody()->toString());
}

?>
Done
--EXPECTREGEX--
Test
(?:string\(46\) "Array\n\(\n    \[test\] \=\> bar\n    \[foo\] \=\> test\n\)\n"\nstring\(46\) "Array\n\(\n    \[test\] \=\> test\n    \[foo\] \=\> bar\n\)\n"\n)+Done
