--TEST--
encodings
--SKIPIF--
<?php
include 'skip.inc';
skipif(!function_exists('http_deflate'), 'need zlib');
?>
--FILE--
<?php
echo "-TEST\n";

set_time_limit(0);
error_reporting(E_ALL);

$s = '';

for ($i = 0; $i < 5000; ++$i) {
	$s .= chr(rand(0,255));
}

var_dump($s == http_inflate(http_deflate($s, HTTP_DEFLATE_TYPE_ZLIB)));
var_dump($s == http_inflate(http_deflate($s, HTTP_DEFLATE_TYPE_GZIP)));
var_dump($s == http_inflate(http_deflate($s, HTTP_DEFLATE_TYPE_RAW)));

if (extension_loaded('zlib')) {
	
	$s = "A simple test string, which won't blow up ext/zlib.\n";
	
	($s == http_inflate(gzencode($s))) or print "GZIP Failed\n";
	($s == http_inflate(gzdeflate($s))) or print "DEFLATE Failed\n";
	($s == http_inflate(gzcompress($s))) or print "COMPRESS Failed\n";
	
	($s == gzinflate(http_deflate($s, HTTP_DEFLATE_TYPE_RAW))) or print "INFLATE Failed\n";
	($s == gzuncompress(http_deflate($s, HTTP_DEFLATE_TYPE_ZLIB))) or print "UNCOMPRESS Failed\n";
}

echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
bool(true)
bool(true)
Done
