--TEST--
message
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

use http\Message as HttpMessage;

try {
    echo new HttpMessage(" gosh\n nosh\n ");
} catch (Exception $ignore) {
}

$m = new HttpMessage();
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_NONE,
	$m->getHeaders()
);

$m = new HttpMessage("GET / HTTP/1.1\r\n");
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_REQUEST,
	$m->getRequestMethod(),
	$m->getRequestUrl(),
	$m->getHeaders()
);

$m = new HttpMessage("HTTP/1.1 200 Okidoki\r\n");
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_RESPONSE,
	$m->getResponseCode(),
	$m->getResponseStatus(),
	$m->getHeaders()
);

echo "---\n";

$m = new HttpMessage(file_get_contents(__DIR__."/data/message_rr_empty.txt"));
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_RESPONSE,
	$m->getResponseCode(),
	$m->getResponseStatus(),
	$m->getHeaders()
);
echo $m->getParentMessage();

$m = new HttpMessage(file_get_contents(__DIR__."/data/message_rr_empty_gzip.txt"));
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_RESPONSE,
	$m->getResponseCode(),
	$m->getResponseStatus(),
	$m->getHeaders()
);
echo $m->getParentMessage();

$m = new HttpMessage(file_get_contents(__DIR__."/data/message_rr_empty_chunked.txt"));
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_RESPONSE,
	$m->getResponseCode(),
	$m->getResponseStatus(),
	$m->getHeaders()
);
echo $m->getParentMessage();

$m = new HttpMessage(file_get_contents(__DIR__."/data/message_rr_helloworld_chunked.txt"));
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_RESPONSE,
	$m->getResponseCode(),
	$m->getResponseStatus(),
	$m->getHeaders()
);
echo $m->getParentMessage();

echo "---\n";

$m = new HttpMessage(fopen(__DIR__."/data/message_rr_empty.txt", "r+b"));
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_RESPONSE,
	$m->getResponseCode(),
	$m->getResponseStatus(),
	$m->getHeaders()
);
echo $m->getParentMessage();

$m = new HttpMessage(fopen(__DIR__."/data/message_rr_empty_gzip.txt", "r+b"));
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_RESPONSE,
	$m->getResponseCode(),
	$m->getResponseStatus(),
	$m->getHeaders()
);
echo $m->getParentMessage();

$m = new HttpMessage(fopen(__DIR__."/data/message_rr_empty_chunked.txt", "r+b"));
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_RESPONSE,
	$m->getResponseCode(),
	$m->getResponseStatus(),
	$m->getHeaders()
);
echo $m->getParentMessage();

$m = new HttpMessage(fopen(__DIR__."/data/message_rr_helloworld_chunked.txt", "r+b"));
echo $m;
var_dump(
	$m->getHttpVersion(),
	$m->getType()==HttpMessage::TYPE_RESPONSE,
	$m->getResponseCode(),
	$m->getResponseStatus(),
	$m->getHeaders()
);
echo $m->getParentMessage();

echo "Done\n";
--EXPECTF--
Test
string(3) "1.1"
bool(true)
array(0) {
}
GET / HTTP/1.1
string(3) "1.1"
bool(true)
string(3) "GET"
string(1) "/"
array(0) {
}
HTTP/1.1 200 Okidoki
string(3) "1.1"
bool(true)
int(200)
string(7) "Okidoki"
array(0) {
}
---
HTTP/1.1 200 OK
Date: Wed, 25 Aug 2010 12:11:44 GMT
Server: Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6
Last-Modified: Wed, 28 Apr 2010 10:54:37 GMT
Etag: "2002a-0-48549d615a35c"
Accept-Ranges: bytes
Content-Length: 0
Vary: Accept-Encoding
Connection: close
Content-Type: text/plain
X-Original-Content-Length: 0
string(3) "1.1"
bool(true)
int(200)
string(2) "OK"
array(10) {
  ["Date"]=>
  string(29) "Wed, 25 Aug 2010 12:11:44 GMT"
  ["Server"]=>
  string(68) "Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6"
  ["Last-Modified"]=>
  string(29) "Wed, 28 Apr 2010 10:54:37 GMT"
  ["Etag"]=>
  string(23) ""2002a-0-48549d615a35c""
  ["Accept-Ranges"]=>
  string(5) "bytes"
  ["Content-Length"]=>
  int(0)
  ["Vary"]=>
  string(15) "Accept-Encoding"
  ["Connection"]=>
  string(5) "close"
  ["Content-Type"]=>
  string(10) "text/plain"
  ["X-Original-Content-Length"]=>
  string(1) "0"
}
GET /default/empty.txt HTTP/1.1
Host: localhost
Connection: close
Content-Length: 0
HTTP/1.1 200 OK
Date: Thu, 26 Aug 2010 09:55:09 GMT
Server: Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6
Last-Modified: Wed, 28 Apr 2010 10:54:37 GMT
Etag: "2002a-0-48549d615a35c"
Accept-Ranges: bytes
Vary: Accept-Encoding
Content-Length: 0
Connection: close
Content-Type: text/plain
X-Original-Content-Length: 20
X-Original-Content-Encoding: gzip
string(3) "1.1"
bool(true)
int(200)
string(2) "OK"
array(11) {
  ["Date"]=>
  string(29) "Thu, 26 Aug 2010 09:55:09 GMT"
  ["Server"]=>
  string(68) "Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6"
  ["Last-Modified"]=>
  string(29) "Wed, 28 Apr 2010 10:54:37 GMT"
  ["Etag"]=>
  string(23) ""2002a-0-48549d615a35c""
  ["Accept-Ranges"]=>
  string(5) "bytes"
  ["Vary"]=>
  string(15) "Accept-Encoding"
  ["Content-Length"]=>
  int(0)
  ["Connection"]=>
  string(5) "close"
  ["Content-Type"]=>
  string(10) "text/plain"
  ["X-Original-Content-Length"]=>
  string(2) "20"
  ["X-Original-Content-Encoding"]=>
  string(4) "gzip"
}
GET /default/empty.txt HTTP/1.1
Host: localhost
Accept-Encoding: gzip
Connection: close
Content-Length: 0
HTTP/1.1 200 OK
Date: Thu, 26 Aug 2010 11:41:02 GMT
Server: Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6
X-Powered-By: PHP/5.3.3
Vary: Accept-Encoding
Connection: close
Content-Type: text/html
X-Original-Transfer-Encoding: chunked
Content-Length: 0
string(3) "1.1"
bool(true)
int(200)
string(2) "OK"
array(8) {
  ["Date"]=>
  string(29) "Thu, 26 Aug 2010 11:41:02 GMT"
  ["Server"]=>
  string(68) "Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6"
  ["X-Powered-By"]=>
  string(9) "PHP/5.3.3"
  ["Vary"]=>
  string(15) "Accept-Encoding"
  ["Connection"]=>
  string(5) "close"
  ["Content-Type"]=>
  string(9) "text/html"
  ["X-Original-Transfer-Encoding"]=>
  string(7) "chunked"
  ["Content-Length"]=>
  int(0)
}
GET /default/empty.php HTTP/1.1
Connection: close
Host: localhost
Content-Length: 0
HTTP/1.1 200 OK
Date: Thu, 26 Aug 2010 12:51:28 GMT
Server: Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6
Vary: Accept-Encoding
Connection: close
Content-Type: text/plain
X-Original-Transfer-Encoding: chunked
Content-Length: 14

Hello, World!
string(3) "1.1"
bool(true)
int(200)
string(2) "OK"
array(7) {
  ["Date"]=>
  string(29) "Thu, 26 Aug 2010 12:51:28 GMT"
  ["Server"]=>
  string(68) "Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6"
  ["Vary"]=>
  string(15) "Accept-Encoding"
  ["Connection"]=>
  string(5) "close"
  ["Content-Type"]=>
  string(10) "text/plain"
  ["X-Original-Transfer-Encoding"]=>
  string(7) "chunked"
  ["Content-Length"]=>
  int(14)
}
GET /cgi-bin/chunked.sh HTTP/1.1
Host: localhost
Connection: close
Content-Length: 0
---
HTTP/1.1 200 OK
Date: Wed, 25 Aug 2010 12:11:44 GMT
Server: Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6
Last-Modified: Wed, 28 Apr 2010 10:54:37 GMT
Etag: "2002a-0-48549d615a35c"
Accept-Ranges: bytes
Content-Length: 0
Vary: Accept-Encoding
Connection: close
Content-Type: text/plain
X-Original-Content-Length: 0
string(3) "1.1"
bool(true)
int(200)
string(2) "OK"
array(10) {
  ["Date"]=>
  string(29) "Wed, 25 Aug 2010 12:11:44 GMT"
  ["Server"]=>
  string(68) "Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6"
  ["Last-Modified"]=>
  string(29) "Wed, 28 Apr 2010 10:54:37 GMT"
  ["Etag"]=>
  string(23) ""2002a-0-48549d615a35c""
  ["Accept-Ranges"]=>
  string(5) "bytes"
  ["Content-Length"]=>
  int(0)
  ["Vary"]=>
  string(15) "Accept-Encoding"
  ["Connection"]=>
  string(5) "close"
  ["Content-Type"]=>
  string(10) "text/plain"
  ["X-Original-Content-Length"]=>
  string(1) "0"
}
GET /default/empty.txt HTTP/1.1
Host: localhost
Connection: close
Content-Length: 0
HTTP/1.1 200 OK
Date: Thu, 26 Aug 2010 09:55:09 GMT
Server: Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6
Last-Modified: Wed, 28 Apr 2010 10:54:37 GMT
Etag: "2002a-0-48549d615a35c"
Accept-Ranges: bytes
Vary: Accept-Encoding
Content-Length: 0
Connection: close
Content-Type: text/plain
X-Original-Content-Length: 20
X-Original-Content-Encoding: gzip
string(3) "1.1"
bool(true)
int(200)
string(2) "OK"
array(11) {
  ["Date"]=>
  string(29) "Thu, 26 Aug 2010 09:55:09 GMT"
  ["Server"]=>
  string(68) "Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6"
  ["Last-Modified"]=>
  string(29) "Wed, 28 Apr 2010 10:54:37 GMT"
  ["Etag"]=>
  string(23) ""2002a-0-48549d615a35c""
  ["Accept-Ranges"]=>
  string(5) "bytes"
  ["Vary"]=>
  string(15) "Accept-Encoding"
  ["Content-Length"]=>
  int(0)
  ["Connection"]=>
  string(5) "close"
  ["Content-Type"]=>
  string(10) "text/plain"
  ["X-Original-Content-Length"]=>
  string(2) "20"
  ["X-Original-Content-Encoding"]=>
  string(4) "gzip"
}
GET /default/empty.txt HTTP/1.1
Host: localhost
Accept-Encoding: gzip
Connection: close
Content-Length: 0
HTTP/1.1 200 OK
Date: Thu, 26 Aug 2010 11:41:02 GMT
Server: Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6
X-Powered-By: PHP/5.3.3
Vary: Accept-Encoding
Connection: close
Content-Type: text/html
X-Original-Transfer-Encoding: chunked
Content-Length: 0
string(3) "1.1"
bool(true)
int(200)
string(2) "OK"
array(8) {
  ["Date"]=>
  string(29) "Thu, 26 Aug 2010 11:41:02 GMT"
  ["Server"]=>
  string(68) "Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6"
  ["X-Powered-By"]=>
  string(9) "PHP/5.3.3"
  ["Vary"]=>
  string(15) "Accept-Encoding"
  ["Connection"]=>
  string(5) "close"
  ["Content-Type"]=>
  string(9) "text/html"
  ["X-Original-Transfer-Encoding"]=>
  string(7) "chunked"
  ["Content-Length"]=>
  int(0)
}
GET /default/empty.php HTTP/1.1
Connection: close
Host: localhost
Content-Length: 0
HTTP/1.1 200 OK
Date: Thu, 26 Aug 2010 12:51:28 GMT
Server: Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6
Vary: Accept-Encoding
Connection: close
Content-Type: text/plain
X-Original-Transfer-Encoding: chunked
Content-Length: 14

Hello, World!
string(3) "1.1"
bool(true)
int(200)
string(2) "OK"
array(7) {
  ["Date"]=>
  string(29) "Thu, 26 Aug 2010 12:51:28 GMT"
  ["Server"]=>
  string(68) "Apache/2.2.16 (Unix) mod_ssl/2.2.16 OpenSSL/1.0.0a mod_fastcgi/2.4.6"
  ["Vary"]=>
  string(15) "Accept-Encoding"
  ["Connection"]=>
  string(5) "close"
  ["Content-Type"]=>
  string(10) "text/plain"
  ["X-Original-Transfer-Encoding"]=>
  string(7) "chunked"
  ["Content-Length"]=>
  int(14)
}
GET /cgi-bin/chunked.sh HTTP/1.1
Host: localhost
Connection: close
Content-Length: 0
Done
