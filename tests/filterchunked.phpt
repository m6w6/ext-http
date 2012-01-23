--TEST--
chunked filter
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php
list($in, $out) = stream_socket_pair(
    STREAM_PF_UNIX,
    STREAM_SOCK_STREAM,
    STREAM_IPPROTO_IP
);
stream_filter_append($in, "http.chunked_decode", STREAM_FILTER_READ);
stream_filter_append($out, "http.chunked_encode", STREAM_FILTER_WRITE,
    array());

$file = file(__FILE__);
foreach($file as $line) {
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
