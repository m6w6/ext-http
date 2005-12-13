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

set_time_limit(0);
error_reporting(E_ALL);

$s = '';

srand(time());
for ($i = 0; $i < 1000000; $i++) {
	$s .= chr(rand(0,255));
}

var_dump($s == http_gzdecode(http_gzencode($s)));
var_dump($s == http_inflate(http_deflate($s)));
var_dump($s == http_inflate(http_deflate($s, -1, true)));

if (extension_loaded('zlib')) {
	
	$s = "A simple test string, which won't blow up ext/zlib.\n";
	
	($s == http_gzdecode(gzencode($s))) or print "GZIP Failed\n";
	($s == http_inflate(gzdeflate($s))) or print "DEFLATE Failed\n";
	($s == http_inflate(gzcompress($s))) or print "COMPRESS Failed\n";
	
	($s == gzinflate(http_deflate($s))) or print "INFLATE Failed\n";
	($s == gzuncompress(http_deflate($s, -1, true))) or print "UNCOMPRESS Failed\n";
}

echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
bool(true)
bool(true)
Done
