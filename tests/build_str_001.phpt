--TEST--
http_build_str
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";

parse_str("a=b", $q);
echo http_build_str($q, null, "&"), "\n";

parse_str("a=b&c[0]=1", $q);
echo http_build_str($q, null, "&"), "\n";

parse_str("a=b&c[0]=1&d[e]=f", $q);
echo http_build_str($q, null, "&"), "\n";

echo http_build_str(array(1,2,array(3)), "foo", "&"), "\n";

echo "Done\n";
?>
--EXPECTF--
%aTEST
a=b
a=b&c%5B0%5D=1
a=b&c%5B0%5D=1&d%5Be%5D=f
foo%5B0%5D=1&foo%5B1%5D=2&foo%5B2%5D%5B0%5D=3
Done
