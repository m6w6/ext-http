--TEST--
response stream ranges
--SKIPIF--
<?php include "skipif.inc"; ?>
--ENV--
HTTP_RANGE=bytes=2-4
--GET--
a=b
--FILE--
<?php
$f = tmpfile();
$r = new http\Env\Response;
$r->setContentType("text/plain");
$r->setContentDisposition(
    array("attachment" => array(array("filename" => basename(__FILE__))))
);
$r->setBody(new http\Message\Body(fopen(__FILE__, "rb")));
$r->send($f);
rewind($f);
var_dump(stream_get_contents($f));
?>
--EXPECTF--
string(%i) "HTTP/1.1 206 Partial Content%c
Accept-Ranges: bytes%c
X-Powered-By: PHP/%s%c
Content-Type: text/plain%c
Content-Range: bytes 2-4/311%c
%c
php"
