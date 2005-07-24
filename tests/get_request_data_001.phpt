--TEST--
get request data
--SKIPIF--
<?php
include 'skip.inc';
?>
--ENV--
HTTP_ACCEPT_CHARSET=iso-8859-1, *
HTTP_USER_AGENT=Mozilla/5.0
--POST--
a=b&c=d
--FILE--
<?php
echo "-TEST\n";
print_r(http_get_request_headers());
var_dump(http_get_request_body());
?>
--EXPECTF--
%sTEST
Array
(
    [Accept-Charset] => iso-8859-1, *
    [User-Agent] => Mozilla/5.0
)
string(7) "a=b&c=d"

