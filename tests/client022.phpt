--TEST--
client http2
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
skip_http2_test();
?>
--FILE--
<?php 

include "helper/server.inc";

echo "Test\n";

nghttpd(function($port) {
	$client = new http\Client;
	$client->setOptions([
		"protocol" => http\Client\Curl\HTTP_VERSION_2_0,
		"ssl" => [
			"cainfo" => __DIR__."/helper/http2.crt",
			"verifypeer" => false, // needed for NSS without PEM support 
		]
	]);
	
	$request = new http\Client\Request("GET", "https://localhost:$port");
	$client->enqueue($request);
	echo $client->send()->getResponse();
});

?>
===DONE===
--EXPECTF--
Test
HTTP/2 200
%a

<!doctype html>
<html>
	<head>
		<meta charset="utf-8">
		<title>HTTP2</title>
	</head>
	<body>
		Nothing to see here.
	</body>
</html>
===DONE===
