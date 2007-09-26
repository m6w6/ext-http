--TEST--
http_match_request_header()
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
?>
--ENV--
HTTP_FOO=bar
--FILE--
<?php
echo "-TEST\n";
var_dump(http_match_request_header("Foo", "bar", 1));
var_dump(http_match_request_header("fOO", "BAR", 0));
var_dump(http_match_request_header("foo", "BAR", 1));
echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
bool(true)
bool(false)
Done
