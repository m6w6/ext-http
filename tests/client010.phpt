--TEST--
client upload
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php

include "helper/server.inc";

echo "Test\n";
$RE =
'/(Array
\(
    \[upload\] \=\> Array
        \(
            \[name\] \=\> client010\.php
            \[type\] \=\> text\/plain
            \[size\] \=\> \d+
        \)

\)
)+/';

server("env.inc", function($port) use($RE) {

	$request = new http\Client\Request("POST", "http://localhost:$port");
	$request->getBody()->addForm(null, array("file"=>__FILE__, "name"=>"upload", "type"=>"text/plain"));

	foreach (http\Client::getAvailableDrivers() as $driver) {
		$client = new http\Client($driver);
		$client->enqueue($request)->send();
		if (!preg_match($RE, $s = $client->getResponse()->getBody()->toString())) {
			echo($s);
		}
	}
});
?>
Done
--EXPECT--
Test
Done
