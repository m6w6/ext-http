--TEST--
object error exception2
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 

echo "Test\n";

class eh extends http\Object {}

$eh = new eh;
$eh->setErrorHandling(http\Object::EH_THROW);
$eh->triggerError(E_USER_WARNING, http\Exception::E_UNKNOWN, "exception");

?>
Done
--EXPECTF--
Test

Fatal error: Uncaught exception 'http\Exception' with message 'exception' in %s:%d
Stack trace:
#0 /home/mike/src/pecl_http-DEV_2.svn/tests/object007.php(9): http\Object->triggerError(512, 0, 'exception')
#1 {main}
  thrown in %s on line %d
