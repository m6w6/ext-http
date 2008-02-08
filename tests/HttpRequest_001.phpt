--TEST--
HttpRequest options
--SKIPIF--
<?php
include 'skip.inc';
checkmin("5.2.5");
checkcls('HttpRequest');
?>
--FILE--
<?php
echo "-TEST\n";
$r1 = new HttpRequest(null, 0, array('redirect'=>11, 'headers'=>array('X-Foo'=>'Bar')));
$r2 = new HttpRequest;
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
%aTEST
Array
(
    [0] => Array
        (
            [headers] => Array
                (
                    [X-Foo] => Bar
                    [X-Bar] => Foo
                )

            [redirect] => 11
        )

    [1] => Array
        (
            [headers] => Array
                (
                    [X-Bar] => Foo
                    [X-Foo] => Bar
                )

            [redirect] => 99
        )

)
bool(false)
