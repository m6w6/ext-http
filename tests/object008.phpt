--TEST--
object normal error2
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

class eh extends http\Object {}

$eh = new eh;
$eh->setErrorHandling(http\Object::EH_NORMAL);
$eh->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "warning");

?>
Done
--EXPECTF--
Test

Warning: http\Object::triggerError(): warning in %s on line %d
Done
