--TEST--
client history
--SKIPIF--
<?php 
include "skipif.inc";
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
POST /ext-http/.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
Content-Length: 6
Content-Type: application/x-www-form-urlencoded
X-Original-Content-Length: 6

foobar
HTTP/1.1 200 OK
Date: %s
Server: %s
Vary: %s
Content-Length: 19
Content-Type: text/html
X-Original-Content-Length: 19

string(6) "foobar"

POST /ext-http/.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
Content-Length: 6
Content-Type: application/x-www-form-urlencoded
X-Original-Content-Length: 6

foobar
HTTP/1.1 200 OK
Date: %s
Server: %s
Vary: %s
Content-Length: 19
Content-Type: text/html
X-Original-Content-Length: 19

string(6) "foobar"

POST /ext-http/.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
Content-Length: 6
Content-Type: application/x-www-form-urlencoded
X-Original-Content-Length: 6

foobar
HTTP/1.1 200 OK
Date: %s
Server: %s
Vary: %s
Content-Length: 19
Content-Type: text/html
X-Original-Content-Length: 19

string(6) "foobar"

Done
