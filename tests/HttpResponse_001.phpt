--TEST--
HttpResponse - send data with caching headers
--SKIPIF--
<?php 
include 'skip.inc';
checkmin("5.2.5");
checkcgi();
?>
--FILE--
<?php
HttpResponse::setCache(true);
HttpResponse::setCacheControl('public', 3600);
HttpResponse::setData('foobar');
HttpResponse::send();
?>
--EXPECTF--
X-Powered-By: PHP/%a
Cache-Control: public, must-revalidate, max-age=3600
Last-Modified: %a, %d %a 20%d %d:%d:%d GMT
Content-Type: %a
Accept-Ranges: bytes
ETag: "3858f62230ac3c915f300c664312c63f"
Content-Length: 6

foobar
