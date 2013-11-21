--TEST--
client response cookie
--SKIPIF--
<?php
include "skipif.inc";
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
(?:array\(7\) \{
  \["cookies"\]\=\>
  array\(2\) \{
    \["foo"\]\=\>
    string\(3\) "bar"
    \["bar"\]\=\>
    string\(3\) "foo"
  \}
  \["extras"\]\=\>
  array\(0\) \{
  \}
  \["flags"\]\=\>
  int\(0\)
  \["expires"\]\=\>
  int\(\-1\)
  \["max\-age"\]\=\>
  int\(\-1\)
  \["path"\]\=\>
  string\(0\) ""
  \["domain"\]\=\>
  string\(0\) ""
\}
)+Done
