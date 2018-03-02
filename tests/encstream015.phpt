--TEST--
encoding stream brotli static
--SKIPIF--
<?php
include "skipif.inc";
class_exists("http\\Encoding\\Stream\\Enbrotli", false) or die("SKIP need brotli support");
?>
--FILE--
<?php
echo "Test\n";

$file = file_get_contents(__FILE__);
var_dump($file ===
	http\Encoding\Stream\Debrotli::decode(
		http\Encoding\Stream\Enbrotli::encode(
			$file, http\Encoding\Stream\Enbrotli::MODE_GENERIC
		)
	)
);
var_dump($file ===
	http\Encoding\Stream\Debrotli::decode(
		http\Encoding\Stream\Enbrotli::encode(
			$file, http\Encoding\Stream\Enbrotli::MODE_TEXT
		)
	)
);
var_dump($file ===
	http\Encoding\Stream\Debrotli::decode(
		http\Encoding\Stream\Enbrotli::encode(
			$file, http\Encoding\Stream\Enbrotli::MODE_FONT
		)
	)
);

?>
DONE
--EXPECT--
Test
bool(true)
bool(true)
bool(true)
DONE
