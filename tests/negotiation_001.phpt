--TEST--
negotiation
--SKIPIF--
<?php
include 'skip.inc';
?>
--ENV--
HTTP_ACCEPT_LANGUAGE=de-AT,de-DE;q=0.8,en-GB;q=0.3,en-US;q=0.2
HTTP_ACCEPT_CHARSET=ISO-8859-1,utf-8;q=0.7,*;q=0.7
--FILE--
<?php
echo "-TEST\n";
$langs = array(
	array('de', 'en', 'es'),
);
$csets = array(
	array('utf-8', 'iso-8859-1'),
);
var_dump(http_negotiate_language($langs[0]));
print_r(http_negotiate_language($langs[0], true));
var_dump(http_negotiate_charset($csets[0]));
print_r(http_negotiate_charset($csets[0], true));
echo "Done\n";
--EXPECTF--
%sTEST
string(2) "de"
Array
(
    [de] => 500
    [en] => 0.15
)
string(10) "iso-8859-1"
Array
(
    [iso-8859-1] => 1000
    [utf-8] => 0.7
)
Done
