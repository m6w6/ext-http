--TEST--
object default error handling
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

var_dump(null === http\Object::getDefaultErrorHandling());
http\Object::setDefaultErrorHandling(http\Object::EH_SUPPRESS);
var_dump(http\Object::EH_SUPPRESS === http\Object::getDefaultErrorHandling());

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
Done
