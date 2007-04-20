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

var_dump(str_repeat("abc", 6000) === http_parse_message(http_put_data("http://dev.iworks.at/ext-http/.print_put.php5", str_repeat("abc", 6000/* > CURLBUF_SIZE */)))->body);

echo "Done\n";
?>
--EXPECTF--
%sTEST
bool(true)
Done
