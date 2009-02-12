--TEST--
HttpResponse - send cached gzipped data
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin("5.2.7");
skipif(!http_support(HTTP_SUPPORT_ENCODINGS), "need zlib support");
?>
--ENV--
HTTP_IF_NONE_MATCH="900150983cd24fb0d6963f7d28e17f72"
HTTP_ACCEPT_ENCODING=gzip
--FILE--
<?php
HttpResponse::setGzip(true);
HttpResponse::setCache(true);
HttpResponse::setCacheControl('public', 3600);
HttpResponse::setData("abc");
HttpResponse::send();
?>
--EXPECTF--
Status: 304%s
X-Powered-By: PHP/%s
Cache-Control: public, must-revalidate, max-age=3600
Last-Modified: %s
Accept-Ranges: bytes
ETag: "900150983cd24fb0d6963f7d28e17f72"
