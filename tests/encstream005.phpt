--TEST--
encoding stream zlib static
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$file = file_get_contents(__FILE__);
var_dump($file ===
	http\Encoding\Stream\Inflate::decode(
		http\Encoding\Stream\Deflate::encode(
			$file, http\Encoding\Stream\Deflate::TYPE_GZIP
		)
	)
);
var_dump($file ===
	http\Encoding\Stream\Inflate::decode(
		http\Encoding\Stream\Deflate::encode(
			$file, http\Encoding\Stream\Deflate::TYPE_ZLIB
		)
	)
);
var_dump($file ===
	http\Encoding\Stream\Inflate::decode(
		http\Encoding\Stream\Deflate::encode(
			$file, http\Encoding\Stream\Deflate::TYPE_RAW
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
