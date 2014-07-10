--TEST--
message body add part
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$temp = new http\Message\Body;
var_dump($temp->getBoundary());
$temp->addPart(new http\Message("This: is a header\n\nand this is the data\n"));
var_dump($temp->getBoundary());
echo $temp;

?>
DONE
--EXPECTF--
Test
NULL
string(%d) "%x.%x"
--%x.%x
This: is a header
Content-Length: 21

and this is the data

--%x.%x--
DONE
