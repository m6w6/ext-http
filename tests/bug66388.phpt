--TEST--
Bug #66388 (Crash on POST with Content-Length:0 and untouched body)
--SKIPIF--
<?php php
include "skipif.inc";
?>
--FILE--
<?php

use http\Client,
	http\Client\Request;

echo "Test\n";

$client = new Client();
$request = new Request(
	'POST',
	'https://api.twitter.com/oauth/request_token',
	array(
		'Content-Length' => 0
	)
);
$client->enqueue($request);
echo $client->send()->getResponse()->getResponseCode();

?>

===DONE===
--EXPECT--
Test
401
===DONE===
