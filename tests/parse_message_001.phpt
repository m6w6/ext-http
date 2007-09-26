--TEST--
http_parse_message()
--SKIPIF--
<?php
include 'skip.inc';
checkurl('www.google.com');
skipif(!http_support(HTTP_SUPPORT_REQUESTS), 'need curl support');
?>
--FILE--
<?php
echo "-TEST\n";
$m = http_parse_message(http_get('http://www.google.com'));
echo $m->body;
echo "Done\n";
--EXPECTF--
%sTEST
<HTML>%sThe document has moved%s</HTML>
Done
