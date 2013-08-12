--TEST--
object error suppression2
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

class eh extends http\Object {}

$eh = new eh;
$eh->setErrorHandling(http\Object::EH_SUPPRESS);
$eh->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "suppress");

?>
Done
--EXPECTF--
Test
Done
