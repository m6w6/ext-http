--TEST--
object unknown error handling
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

http\Object::setDefaultErrorHandling(12345);

class eh extends http\Object {}

$eh = new eh;
$eh->setErrorHandling(12345);

?>
Done
--EXPECTF--
Test

Warning: http\Object::setDefaultErrorHandling(): unknown error handling code (12345) in %s on line %d

Warning: http\Object::setErrorHandling(): unknown error handling code (12345) in %s on line %d
Done
