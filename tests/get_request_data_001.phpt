--TEST--
get request data
--SKIPIF--
<?php
include 'skip.inc';
?>
--POST--
a=b&c=d
--FILE--
<?php
echo "-TEST\n";

$_SERVER['HTTP_ACCEPT_CHARSET'] = 'iso-8859-1, *';
$_SERVER['HTTP_ACCEPT_ENCODING'] = 'none';
$_SERVER['HTTP_USER_AGENT'] = 'Mozilla/5.0';
$_SERVER['HTTP_HOST'] = 'localhost';

$h = http_get_request_headers();
ksort($h);
print_r($h);
var_dump(http_get_request_body());
var_dump(http_get_request_body());
var_dump(http_get_request_body());
var_dump(fread(http_get_request_body_stream(), 4096));
echo "Done\n";
?>
--EXPECTF--
%aTEST
Array
(
    [Accept-Charset] => iso-8859-1, *
    [Accept-Encoding] => none
    [Host] => localhost
    [User-Agent] => Mozilla/5.0
)
string(7) "a=b&c=d"
string(7) "a=b&c=d"
string(7) "a=b&c=d"
string(7) "a=b&c=d"
Done
