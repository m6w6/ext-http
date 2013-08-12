--TEST--
object normal error
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

class eh extends http\Object {}

http\Object::setDefaultErrorHandling(http\Object::EH_NORMAL);
$o = new eh;
$o->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "warning");

?>
Done
--EXPECTF--
Test

Warning: http\Object::triggerError(): warning in %s on line %d
Done
