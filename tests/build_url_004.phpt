--TEST--
http_build_url flags
--SKPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";
echo http_build_url("http://mike@www.example.com/foo/bar", "./baz", HTTP_URL_STRIP_AUTH|HTTP_URL_JOIN_PATH), "\n";
echo http_build_url("http://mike@www.example.com/foo/bar/", "../baz", HTTP_URL_STRIP_USER|HTTP_URL_JOIN_PATH), "\n";
echo http_build_url("http://mike:1234@www.example.com/foo/bar/", "./../baz", HTTP_URL_STRIP_PASS|HTTP_URL_JOIN_PATH), "\n";
echo http_build_url("http://www.example.com:8080/foo?a=b#frag", "?b=c", HTTP_URL_JOIN_QUERY|HTTP_URL_STRIP_PORT|HTTP_URL_STRIP_FRAGMENT|HTTP_URL_STRIP_PATH), "\n";
echo "Done\n";
?>
--EXPECTF--
%sTEST
http://www.example.com/foo/baz
http://www.example.com/foo/baz
http://mike@www.example.com/foo/baz
http://www.example.com/?a=b&b=c
Done