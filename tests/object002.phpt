--TEST--
object instance error handling
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

class eh extends http\Object {}

$eh = new eh;
var_dump(null === $eh->getErrorHandling());
$eh->setErrorHandling(eh::EH_SUPPRESS);
var_dump(eh::EH_SUPPRESS === $eh->getErrorHandling());

?>
Done
--EXPECT--
Test
bool(true)
bool(true)
Done
