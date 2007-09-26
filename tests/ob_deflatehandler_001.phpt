--TEST--
ob_deflatehandler
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin(5);
skipif(!http_support(HTTP_SUPPORT_ENCODINGS), "need zlib");
?>
--ENV--
HTTP_ACCEPT_ENCODING=gzip
--FILE--
<?php
ob_start('ob_deflatehandler');
echo "-TEST\n";
echo "Done\n";
?>
--EXPECTF--
%s
Content-Encoding: gzip
Vary: Accept-Encoding
%s

