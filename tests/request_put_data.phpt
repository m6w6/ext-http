--TEST--
http_put_data()
--SKIPIF--
<?php
include 'skip.inc';
skipif(!http_support(HTTP_SUPPORT_REQUESTS), "need request support");
?>
--FILE--
<?php
echo "-TEST\n";

$data = str_repeat("abc", 6000/* > CURLBUF_SIZE */);
$resp = http_put_data("http://dev.iworks.at/ext-http/.print_put.php5", $data);
$mess = http_parse_message($resp);
var_dump($data === $mess->body);

echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
Done
