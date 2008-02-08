--TEST--
HttpResponse - send gzipped file with caching headers
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
HttpResponse::setCache(true);
HttpResponse::setCacheControl('public', 3600);
HttpResponse::setFile(__FILE__);
HttpResponse::send();
?>
--EXPECTF--
X-Powered-By: PHP/%a
Cache-Control: public, must-revalidate, max-age=3600
Last-Modified: %a, %d %a 20%d %d:%d:%d GMT
Content-Type: %a
Accept-Ranges: bytes
ETag: "%a"
Content-Encoding: gzip
Vary: Accept-Encoding

%a
