--TEST--
env response send failure
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

class closer extends php_user_filter {
	function filter ($in, $out, &$consumed, $closing) {
		while ($bucket = stream_bucket_make_writeable($in)) {
			stream_bucket_append($out, $bucket);
		}
		return PSFS_ERR_FATAL;
	}
}

stream_filter_register("closer", "closer");

$r = new http\Env\Response;
$r->getBody()->append(str_repeat("a", 16*1024*4));
$s = fopen("php://temp", "w");
stream_filter_append($s, "closer");
var_dump($r->send($s));
?>
DONE
--EXPECTF--
Test

Warning: http\Env\Response::send(): Failed to send response body in %s on line %d
bool(false)
DONE
