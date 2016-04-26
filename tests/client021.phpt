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

function dump($f) {
	return;
	readfile($f);
}

$tmpfile = tempnam(sys_get_temp_dir(), "cookie.");
$request = new http\Client\Request("GET", "http://localhost");
$request->setOptions(array("cookiestore" => $tmpfile));

server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	echo $client->requeue($request)->send()->getResponse();
#dump($tmpfile);
	echo $client->requeue($request)->send()->getResponse();
#dump($tmpfile);
	echo $client->requeue($request)->send()->getResponse();
#dump($tmpfile);
});
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port));
	$client = new http\Client;
	echo $client->requeue($request)->send()->getResponse();
#dump($tmpfile);
	echo $client->requeue($request)->send()->getResponse();
#dump($tmpfile);
	echo $client->requeue($request)->send()->getResponse();
#dump($tmpfile);
});
	
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port, "cookiesession" => true));
	$client = new http\Client;
	echo $client->requeue($request)->send()->getResponse();
dump($tmpfile);
	echo $client->requeue($request)->send()->getResponse();
dump($tmpfile);
	echo $client->requeue($request)->send()->getResponse();
dump($tmpfile);
});
	
server("cookie.inc", function($port) use($request, $tmpfile) {
	$request->setOptions(array("port" => $port, "cookiesession" => false));
	$client = new http\Client;
	echo $client->requeue($request)->send()->getResponse();
dump($tmpfile);
	echo $client->requeue($request)->send()->getResponse();
dump($tmpfile);
	echo $client->requeue($request)->send()->getResponse();
dump($tmpfile);
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
