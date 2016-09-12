--TEST--
Bug #73055 (Type confusion vulnerability in merge_param())
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

try {
	echo new http\QueryString("[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]][[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[&%C0[]E[=&2[&%C0[]E[16706[*[");
} catch (Exception $e) {
	echo $e;
}
?>

===DONE===
--EXPECTF--
Test
%r(exception ')?%rhttp\Exception\BadQueryStringException%r(' with message '|: )%rhttp\QueryString::__construct(): Max input nesting level of %d exceeded%r'?%r in %sbug73055.php:%d
Stack trace:
#0 %sbug73055.php(%d): http\QueryString->__construct('[[[[[[[[[[[[[[[...')
#1 {main}
===DONE===
