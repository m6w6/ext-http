--TEST--
message body stat
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$file = new http\Message\Body(fopen(__FILE__, "r"));
$temp = new http\Message\Body();
var_dump(filesize(__FILE__) === $file->stat("size"));
var_dump(filemtime(__FILE__) === $file->stat("mtime"));
var_dump(fileatime(__FILE__) === $file->stat("atime"));
var_dump(filectime(__FILE__) === $file->stat("ctime"));
var_dump(
	(object) array(
		"size" => 0,
		"mtime" => 0,
		"atime" => 0,
		"ctime" => 0,
	) == $temp->stat()
);
?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
DONE
