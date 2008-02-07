--TEST--
content range message
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";

$message = 
"HTTP/1.1 200 Ok\n".
"Content-Range: bytes: 0-1/5\n\n".
"OK\n";

$msg = http_parse_message($message);
var_dump($msg->body);

$message = 
"HTTP/1.1 200 Ok\n".
"Content-Range: bytes 0-1/1\n\n".
"X\n";

$msg = http_parse_message($message);

echo "Done\n";
--EXPECTF--
%aTEST
string(2) "OK"
%a Invalid Content-Range header: bytes 0-1/1 in%a
Done
