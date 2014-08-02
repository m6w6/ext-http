--TEST--
message body resource
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$file = new http\Message\Body(fopen(__FILE__,"r"));
for($i=0;$i<10;++$i) $stream = $file->getResource();
var_dump(is_resource($stream));
$stat = fstat($stream);
var_dump(filesize(__FILE__) === $stat["size"]);

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
DONE
