--TEST--
client history
--SKIPIF--
<?php 
include "skipif.inc";
skip_online_test();
skip_client_test();
?>
--FILE--
<?php 

echo "Test\n";

$body = new http\Message\Body;
$body->append("foobar");

$request = new http\Client\Request;
$request->setBody($body);
$request->setRequestMethod("POST");
$request->setRequestUrl("http://dev.iworks.at/ext-http/.print_request.php");

$client = new http\Client;
$client->recordHistory = true;

$client->enqueue($request)->send();
echo $client->getHistory()->toString(true);

$client->requeue($request)->send();
echo $client->getHistory()->toString(true);

?>
Done
--EXPECTF--
Test
POST http://dev.iworks.at/ext-http/.print_request.php HTTP/1.1
Content-Length: 6

foobar
HTTP/1.1 200 OK
Vary: %s
Content-Type: text/html
Date: %s
Server: %s
X-Original-Transfer-Encoding: chunked
Content-Length: 19

string(6) "foobar"

POST http://dev.iworks.at/ext-http/.print_request.php HTTP/1.1
Content-Length: 6

foobar
HTTP/1.1 200 OK
Vary: %s
Content-Type: text/html
Date: %s
Server: %s
X-Original-Transfer-Encoding: chunked
Content-Length: 19

string(6) "foobar"

POST http://dev.iworks.at/ext-http/.print_request.php HTTP/1.1
Content-Length: 6

foobar
HTTP/1.1 200 OK
Vary: %s
Content-Type: text/html
Date: %s
Server: %s
X-Original-Transfer-Encoding: chunked
Content-Length: 19

string(6) "foobar"

Done
