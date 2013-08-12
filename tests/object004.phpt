--TEST--
object error exception
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

class eh extends http\Object {}

http\Object::setDefaultErrorHandling(http\Object::EH_THROW);
$o = new eh;
$o->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "exception");

?>
Done
--EXPECTF--
Test

Fatal error: Uncaught exception 'http\Exception' with message 'exception' in %s:%d
Stack trace:
#0 %s(%d): http\Object->triggerError(512, 0, 'exception')
#1 {main}
  thrown in %s on line %d
