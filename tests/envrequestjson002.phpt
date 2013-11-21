--TEST--
env request json
--SKIPIF--
<?php
include "skipif.inc";
_ext("json");
?>
--PUT--
Content-Type: application/json

{"foo": "bar", "a": [1,2,3]}
--FILE--
<?php
echo "Test\n";
print_r($_POST);
?>
Done
--EXPECT--
Test
Array
(
    [foo] => bar
    [a] => Array
        (
            [0] => 1
            [1] => 2
            [2] => 3
        )

)
Done

