--TEST--
http_parse_message()
--SKIPIF--
include 'skip.inc';
checkurl('www.google.com');
--FILE--
<?php
echo "-TEST\n";
echo http_parse_message(http_get('http://www.google.com'))->body;
echo "Done\n";
--EXPECTF--
%sTEST
<HTML>%sThe document has moved%s</HTML>
Done
