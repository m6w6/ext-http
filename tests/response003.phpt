--TEST--
response ranges
--SKIPIF--
<?php include "skipif.inc"; ?>
--ENV--
HTTP_RANGE=bytes=2-4
--GET--
a=b
--FILE--
<?php

$r = new http\Env\Response;
$r->setContentType("text/plain");
$r->setContentDisposition(
    array("attachment" => array(array("filename" => basename(__FILE__))))
);
$r->setBody(new http\Message\Body(fopen(__FILE__, "rb")));
$r->send();

?>
--EXPECTHEADERS--
Content-Type: text/plain
--EXPECTF--
php
