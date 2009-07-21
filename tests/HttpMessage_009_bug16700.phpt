--TEST--
Bug #16700 - child classes of HttpMessage cannot not have array properties
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
?>
--FILE--
<?php
echo "-TEST\n";

class ChildMessage extends HttpMessage {
    public $properties = array();
}

$child = new ChildMessage;
$child->properties['foo'] = 'bar';
echo $child->properties['foo'], "\n";
echo "Done\n";
?>
--EXPECTF--
%aTEST
bar
Done
