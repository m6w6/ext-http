--TEST--
HttpRequest options
--SKIPIF--
<?php 
include 'skip.inc';
(5 > (int) PHP_VERSION) and die('skip PHP5 is required for Http classes');
?>
--FILE--
<?php
$r1 = new HttpRequest;
$r2 = new HttpRequest;
$r1->setOptions(array('redirect'=>11, 'headers'=>array('X-Foo'=>'Bar')));
$r2->setOptions(array('redirect'=>99, 'headers'=>array('X-Bar'=>'Foo')));
$o1 = $r1->getOptions();
$o2 = $r2->getOptions();
$r1->setOptions($o2);
$r2->setOptions($o1);
print_r(array($o1, $o2));
var_dump(serialize($r1->getOptions()) === serialize($r2->getOptions()));
$r1 = null;
$r2 = null;
?>
--EXPECTF--
Content-type: text/html
X-Powered-By: PHP/%s

Array
(
    [0] => Array
        (
            [redirect] => 11
            [headers] => Array
                (
                    [X-Foo] => Bar
                    [X-Bar] => Foo
                )

        )

    [1] => Array
        (
            [redirect] => 99
            [headers] => Array
                (
                    [X-Bar] => Foo
                    [X-Foo] => Bar
                )

        )

)
bool(false)