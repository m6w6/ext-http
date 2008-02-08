--TEST--
HttpResponse - send gzipped file
--SKIPIF--
<?php
include 'skip.inc';
checkmin("5.2.5");
checkcgi();
skipif(!http_support(HTTP_SUPPORT_ENCODINGS), "need zlib support");
?>
--ENV--
HTTP_ACCEPT_ENCODING=gzip
--FILE--
<?php
HttpResponse::setGzip(true);
HttpResponse::setFile(__FILE__);
HttpResponse::send();
?>
--EXPECTF--
X-Powered-By: PHP/%a
Content-Type: %a
Accept-Ranges: bytes
Content-Encoding: gzip
Vary: Accept-Encoding

%a
