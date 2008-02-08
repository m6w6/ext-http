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
echo http_build_url("http://www.example.com:8080/foo?a[0]=b#frag", "?a[0]=1&b=c&a[1]=b", HTTP_URL_JOIN_QUERY|HTTP_URL_STRIP_PORT|HTTP_URL_STRIP_FRAGMENT|HTTP_URL_STRIP_PATH), "\n";
echo "Done\n";
?>
--EXPECTF--
%aTEST
http://www.example.com/foo/baz
http://www.example.com/foo/baz
http://mike@www.example.com/foo/baz
http://www.example.com/?a%5B0%5D=1&a%5B1%5D=b&b=c
Done
