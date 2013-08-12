--TEST--
object error suppression
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

class eh extends http\Object {}

http\Object::setDefaultErrorHandling(http\Object::EH_SUPPRESS);
$o = new eh;
$o->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "suppress");

?>
Done
--EXPECT--
Test
Done
