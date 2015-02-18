--TEST--
message content range
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 
echo "Test\n";
echo new http\Message(fopen(__DIR__."/data/message_r_content_range.txt", "r"));
?>
===DONE===
--EXPECT--
Test
PUT / HTTP/1.1
User-Agent: PECL_HTTP/2.3.0dev PHP/5.6.6-dev libcurl/7.41.0-DEV
Host: localhost:8000
Accept: */*
Expect: 100-continue
Content-Length: 3
Content-Range: bytes 1-2/3
X-Original-Content-Length: 3

23===DONE===
