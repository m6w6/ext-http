--TEST--
URL barfs on punycode
--SKIPIF--
<?php
include "./skipif.inc";
?>
--FILE--
<?php 
echo "Test\n";
echo new http\Url("http://www.xn--kln-sna.de"), "\n";
?>
===DONE===
--EXPECT--
Test
http://www.xn--kln-sna.de/
===DONE===
