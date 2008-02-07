--TEST--
ob_inflatehandler
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
skipif(!http_support(HTTP_SUPPORT_ENCODINGS), "need zlib");
?>
--FILE--
<?php
ob_start('ob_inflatehandler');
echo http_deflate("TEST\n");
?>
--EXPECTF--
%aTEST

