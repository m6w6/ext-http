--TEST--
reponse callback
--SKIPIF--
<?php
include "skipif.inc";
?>
--GET--
dummy=1
--FILE--
<?php

$r = new http\Env\Response;
$r->setCacheControl("public,must-revalidate,max-age=0");
$r->setThrottleRate(1, 0.1);
ob_start($r);

echo "foo";
echo "bar";

ob_end_flush();
$r->send();
--EXPECTHEADERS--
Accept-Ranges: bytes
Cache-Control: public,must-revalidate,max-age=0
ETag: "9ef61f95"
--EXPECTF--
foobar
