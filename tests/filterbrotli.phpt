--TEST--
brotli filter
--SKIPIF--
<?php 
include "skipif.inc";
class_exists("http\\Encoding\\Stream\\Enbrotli", false) or die("SKIP need brotli support");
?>
--FILE--
<?php
list($in, $out) = stream_socket_pair(
    STREAM_PF_UNIX,
    STREAM_SOCK_STREAM,
    STREAM_IPPROTO_IP
);
stream_filter_append($in, "http.brotli_decode", STREAM_FILTER_READ);
stream_filter_append($out, "http.brotli_encode", STREAM_FILTER_WRITE,
    http\Encoding\Stream\Enbrotli::LEVEL_MAX);

$file = file(__FILE__);
foreach ($file as $line) {
    fwrite($out, $line);
    fflush($out);
}
fclose($out);
if (implode("",$file) !== ($read = fread($in, filesize(__FILE__)))) {
    echo "got: $read\n";
}
fclose($in);
?>
DONE
--EXPECT--
DONE
