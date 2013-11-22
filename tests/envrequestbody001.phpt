--TEST--
env request body
--SKIPIF--
<?php include "skipif.inc"; ?>
--PUT--
Content-Type: skip/me
foo
--FILE--
<?php
var_dump((string) \http\Env::getRequestBody());
?>
DONE
--EXPECT--
string(3) "foo"
DONE
