--TEST--
client response cookies
--SKIPIF--
<?php
include "skipif.inc";
skip_online_test();
skip_client_test();
?>
--FILE--
<?php
echo "Test\n";

$request = new http\Client\Request("GET", "http://dev.iworks.at/ext-http/.cookie.php");

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
(?:array\(7\) \{\n  \["cookies"\]\=\>\n  array\(1\) \{\n    \["temp"\]\=\>\n    string\(1\d\) "\d+\.\d+"\n  \}\n  \["extras"\]\=\>\n  array\(0\) \{\n  \}\n  \["flags"\]\=\>\n  int\(0\)\n  \["expires"\]\=\>\n  int\(\-1\)\n  \["max\-age"\]\=\>\n  int\(\-1\)\n  \["path"\]\=\>\n  string\(0\) ""\n  \["domain"\]\=\>\n  string\(0\) ""\n\}\narray\(7\) \{\n  \["cookies"\]\=\>\n  array\(1\) \{\n    \["perm"\]\=\>\n    string\(1\d\) "\d+\.\d+"\n  \}\n  \["extras"\]\=\>\n  array\(0\) \{\n  \}\n  \["flags"\]\=\>\n  int\(0\)\n  \["expires"\]\=\>\n  int\(\d+\)\n  \["max\-age"\]\=\>\n  int\(\-1\)\n  \["path"\]\=\>\n  string\(0\) ""\n  \["domain"\]\=\>\n  string\(0\) ""\n\}\n)+Done
