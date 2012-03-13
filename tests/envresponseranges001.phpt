--TEST--
ranges
--SKIPIF--
<? include "skipif.php";
--XFAIL--
line endings
--GET--
a=b
--ENV--
HTTP_RANGE=bytes=-3,000-001,1-1,0-0,100-
--FILE--
<?php

$r = new http\Env\Response;
$r->setBody(new http\Message\Body(fopen(__FILE__, "rb")));
$r->send();

?>
--EXPECTF--
--%s
Content-Type: application/octet-stream
Content-Range: bytes 107-109/110

?>

--%s
Content-Type: application/octet-stream
Content-Range: bytes 0-1/110

<?
--%s
Content-Type: application/octet-stream
Content-Range: bytes 1-1/110

?
--%s
Content-Type: application/octet-stream
Content-Range: bytes 0-0/110

<
--%s
Content-Type: application/octet-stream
Content-Range: bytes 100-109/110

nd();

?>

--%s--