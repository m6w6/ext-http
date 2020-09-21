--TEST--
phpinfo
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";
phpinfo(INFO_MODULES);
?>
Done
--EXPECTF--
Test
%a
HTTP Support => enabled
Extension Version => 4.%s
%a
Done
