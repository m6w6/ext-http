--TEST--
http_build_url() with relative paths
--SKIPIF--
<?php
include 'skip.inc';
?>
--FILE--
<?php
echo "-TEST\n";
echo http_build_url('page'), "\n";
echo http_build_url('with/some/path/'), "\n";
?>
--EXPECTF--
%sTEST
http://localhost/page
http://localhost/with/some/path/

