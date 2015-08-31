--TEST--
crash when query string has nested array keys
--SKIPIF--
<?php
include "skipif.inc";
php_version_compare(PHP_VERSION, "7.0.0-dev") or die("skip php<7");
?>
--FILE--
<?php

echo "Test\n";

$q = new http\QueryString("a[0][0]=x&a[1][0]=x");
printf("%s\n", $q);

?>
===DONE===
--EXPECT--
Test
a%5B0%5D%5B0%5D=x&a%5B1%5D%5B0%5D=x
===DONE===
