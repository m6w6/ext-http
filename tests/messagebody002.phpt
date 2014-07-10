--TEST--
message body append
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$temp = new http\Message\Body();
$temp->append("yes");

var_dump((string) $temp);

?>
DONE
--EXPECT--
Test
string(3) "yes"
DONE
