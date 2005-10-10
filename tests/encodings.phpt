--TEST--
encodings
--SKIPIF--
<?php
include 'skip.inc';
skipif(!function_exists('http_gzencode'), 'need zlib');
?>
--FILE--
<?php
echo "-TEST\n";

error_reporting(E_ALL);

$s = '';

srand(time());
for ($i = 0; $i < 1000000; $i++) {
	$s .= chr(rand(0,255));
}

/* cannot test ext/zlib against this generated data, 
   because it will fail with such widely differing binary data */

var_dump($s == http_gzdecode(http_gzencode($s)));
var_dump($s == http_inflate(http_deflate($s)));
var_dump($s == http_uncompress(http_compress($s)));


$s = "A simple test string, which won't blow up ext/zlib.\n";

var_dump($s == http_gzdecode(gzencode($s)));
var_dump($s == http_inflate(gzdeflate($s)));
var_dump($s == http_uncompress(gzcompress($s)));

/* no gzdecode in ext/zlib
var_dump($s == gzdecode(http_gzencode($s))); */
var_dump($s == gzinflate(http_deflate($s)));
var_dump($s == gzuncompress(http_compress($s)));

if (extension_loaded('zlib')) {
	(gzencode($s) == http_gzencode($s)) or print "GZIP Failed\n";
	(gzdeflate($s) == http_deflate($s)) or print "DEFLATE Failed\n";
	(gzcompress($s) == http_compress($s)) or print "COMPRESS Failed\n";
}

echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Done
