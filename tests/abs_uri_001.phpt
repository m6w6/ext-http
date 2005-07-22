--TEST--
http_absolute_uri() with relative paths
--SKIPIF--
<?php 
include 'skip.inc';
?>
--FILE--
<?php
echo http_absolute_uri('page'), "\n";
echo http_absolute_uri('with/some/path/'), "\n";
?>
--EXPECTF--
%shttp://localhost/page
http://localhost/with/some/path/
