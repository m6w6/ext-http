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
echo "Done\n";
?>
--EXPECTF--
%sTEST
http://%s/page
http://%s/with/some/path/
Done
