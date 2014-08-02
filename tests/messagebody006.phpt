--TEST--
message body etag
--SKIPIF--
<?php
include "skipif.inc";
?>
--INI--
http.etag.mode = crc32b
--FILE--
<?php
echo "Test\n";

$file = new http\Message\Body(fopen(__FILE__, "r"));
$temp = new http\Message\Body;
$s = stat(__FILE__);
var_dump(
	sprintf(
		"%lx-%lx-%lx", 
		$s["ino"],$s["mtime"],$s["size"]
	) === $file->etag()
);
var_dump($temp->etag());

?>
DONE
--EXPECT--
Test
bool(true)
string(8) "00000000"
DONE
