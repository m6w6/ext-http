--TEST--
client upload
--SKIPIF--
<?php
include "skipif.inc";
skip_online_test();
skip_client_test();
?>
--FILE--
<?php
echo "Test\n";

$RE =
'/(Array
\(
    \[upload\] \=\> Array
        \(
            \[name\] \=\> client010\.php
            \[type\] \=\> text\/plain
            \[tmp_name\] \=\> .+
            \[error\] \=\> 0
            \[size\] \=\> \d+
        \)

\)
)+/';
$request = new http\Client\Request("POST", "http://dev.iworks.at/ext-http/.print_request.php");
$request->getBody()->addForm(null, array("file"=>__FILE__, "name"=>"upload", "type"=>"text/plain"));

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	$client->enqueue($request)->send();
	if (!preg_match($RE, $s = $client->getResponse()->getBody()->toString())) {
		echo($s);
	}
}
?>
Done
--EXPECT--
Test
Done