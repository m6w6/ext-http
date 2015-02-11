--TEST--
client response cookie
--SKIPIF--
<?php
include "skipif.inc";
skip_online_test();
skip_client_test();
?>
--FILE--
<?php
echo "Test\n";

$request = new http\Client\Request("GET", "http://dev.iworks.at/ext-http/.cookie1.php");

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	foreach($client->enqueue($request)->send()->getResponse()->getCookies(0, array("comment")) as $cookies) {
		var_dump($cookies->toArray());
	}
}
?>
Done
--EXPECTREGEX--
Test
(?:array\(7\) \{\n  \["cookies"\]\=\>\n  array\(2\) \{\n    \["foo"\]\=\>\n    string\(3\) "bar"\n    \["bar"\]\=\>\n    string\(3\) "foo"\n  \}\n  \["extras"\]\=\>\n  array\(0\) \{\n  \}\n  \["flags"\]\=\>\n  int\(0\)\n  \["expires"\]\=\>\n  int\(\-1\)\n  \["max\-age"\]\=\>\n  int\(\-1\)\n  \["path"\]\=\>\n  string\(0\) ""\n  \["domain"\]\=\>\n  string\(0\) ""\n\}\n)+Done
