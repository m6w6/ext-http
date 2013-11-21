--TEST--
client upload
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$request = new http\Client\Request("POST", "http://dev.iworks.at/ext-http/.print_request.php");
$request->getBody()->addForm(null, array("file"=>__FILE__, "name"=>"upload", "type"=>"text/plain"));

foreach (http\Client::getAvailableDrivers() as $driver) {
	$client = new http\Client($driver);
	$client->enqueue($request)->send();
	var_dump($client->getResponse()->getBody()->toString());
}
?>
Done
--EXPECTREGEX--
Test
(?:string\(\d+\) "Array
\(
    \[upload\] \=\> Array
        \(
            \[name\] \=\> client010\.php
            \[type\] \=\> text\/plain
            \[tmp_name\] \=\> .*
            \[error\] \=\> 0
            \[size\] \=\> \d+
        \)

\)
"
)+Done
