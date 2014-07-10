--TEST--
message body to stream
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$file = new http\Message\Body(fopen(__FILE__,"r"));
$file->toStream($f = fopen("php://temp", "w")); 
fseek($f, 0, SEEK_SET);
var_dump(file_get_contents(__FILE__) === stream_get_contents($f));

?>
DONE
--EXPECT--
Test
bool(true)
DONE
