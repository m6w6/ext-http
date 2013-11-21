--TEST--
env request body
--SKIPIF--
<? include "skipif.inc";
--PUT--
Content-Type: skip/me
foo
--FILE--
<?
var_dump((string) \http\Env::getRequestBody());
?>
DONE
--EXPECT--
string(3) "foo"
DONE
