--TEST--
HttpResponse - send cached gzipped data
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5.1);
skipif(!http_support(HTTP_SUPPORT_ENCODINGS), "need zlib support");
?>
--FILE--
<?php
$_SERVER["HTTP_IF_NONE_MATCH"] = '"900150983cd24fb0d6963f7d28e17f72"';
$_SERVER["HTTP_ACCEPT_ENCODING"] = "gzip";
HttpResponse::setGzip(true);
HttpResponse::setCache(true);
HttpResponse::setCacheControl('public', 3600);
HttpResponse::setData("abc");
HttpResponse::send();
?>
--EXPECTF--
Status: 304
X-Powered-By: PHP/%s
Cache-Control: public, must-revalidate, max-age=3600
Last-Modified: %s
Content-Type: %s
Accept-Ranges: bytes
ETag: "900150983cd24fb0d6963f7d28e17f72"
