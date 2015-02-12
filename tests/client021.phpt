--TEST--
client cookies
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php 

include "helper/server.inc";

echo "Test\n";

$tmpfile = tempnam(sys_get_temp_dir(), "cookie.");
$request = new http\Client\Request("GET", "http://localhost");
$request->setOptions(["cookiestore" => $tmpfile]);

server("cookie.inc", function($port) use($request) {
	$request->setOptions(["port" => $port]);
	$client = new http\Client;
	echo $client->requeue($request)->send()->getResponse();
	echo $client->requeue($request)->send()->getResponse();
	echo $client->requeue($request)->send()->getResponse();
});

server("cookie.inc", function($port) use($request) {
	$request->setOptions(["port" => $port]);
	$client = new http\Client;
	echo $client->requeue($request)->send()->getResponse();
	echo $client->requeue($request)->send()->getResponse();
	echo $client->requeue($request)->send()->getResponse();
});

server("cookie.inc", function($port) use($request) {
	$request->setOptions(["port" => $port, "cookiesession" => true]);
	$client = new http\Client;
	echo $client->requeue($request)->send()->getResponse();
	echo $client->requeue($request)->send()->getResponse();
	echo $client->requeue($request)->send()->getResponse();
});

server("cookie.inc", function($port) use($request) {
	$request->setOptions(["port" => $port, "cookiesession" => false]);
	$client = new http\Client;
	echo $client->requeue($request)->send()->getResponse();
	echo $client->requeue($request)->send()->getResponse();
	echo $client->requeue($request)->send()->getResponse();
});

unlink($tmpfile);

?>
===DONE===
--EXPECT--
Test
HTTP/1.1 200 OK
Set-Cookie: counter=1;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=2;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=3;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=4;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=5;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=6;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=1;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=1;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=1;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=2;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=3;
Etag: ""
X-Original-Transfer-Encoding: chunked
HTTP/1.1 200 OK
Set-Cookie: counter=4;
Etag: ""
X-Original-Transfer-Encoding: chunked
===DONE===
