--TEST--
stream filter fun
--SKIPIF--
<?php
include 'skip.inc';
checkmin("5.2.5");
skipif(!http_support(HTTP_SUPPORT_ENCODINGS), "need zlib");
?>
--FILE--
<?php
echo "-TEST\n";

define('OUT', fopen('php://output', 'w'));

stream_filter_append(OUT, 'http.chunked_encode');
stream_filter_append(OUT, 'http.deflate');
stream_filter_append(OUT, 'http.chunked_encode');
stream_filter_append(OUT, 'http.deflate');
stream_filter_append(OUT, 'http.inflate');
stream_filter_append(OUT, 'http.chunked_decode');
stream_filter_append(OUT, 'http.inflate');
stream_filter_append(OUT, 'http.chunked_decode');

$text = <<<SOME_TEXT
This is some stream filter fun; we'll see if it bails out or not.
The text should come out at the other end of the stream exactly like written to it.
Go figure!
SOME_TEXT;

srand(time());
foreach (str_split($text, 5) as $part) {
	fwrite(OUT, $part);
	if (rand(0, 1)) {
		fflush(OUT);
	}
}
?>
--EXPECTF--
%aTEST
This is some stream filter fun; we'll see if it bails out or not.
The text should come out at the other end of the stream exactly like written to it.
Go figure!
