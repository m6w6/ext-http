--TEST--
request etag
--SKIPIF--
<?php
include 'skip.inc';
skipif(!http_support(HTTP_SUPPORT_REQUESTS), "need request support");
?>
--FILE--
<?php
echo "-TEST\n";
var_dump(http_get("http://dev.iworks.at/ext-http/etag", array("etag" => '"26ad3a-5-95eb19c0"')));
echo "Done\n";
?>
--EXPECTF--
%aTEST
string(%d) "HTTP/1.1 304 Not Modified
Date: %a
Server: %a
ETag: "26ad3a-5-95eb19c0"
"
Done