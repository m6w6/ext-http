--TEST--
client static cookies
--SKIPIF--
<?php
include "skipif.inc";
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
(?:string\(46\) "Array
\(
    \[test\] \=\> bar
    \[foo\] \=\> test
\)
"
string\(46\) "Array
\(
    \[test\] \=\> test
    \[foo\] \=\> bar
\)
"
)+Done
