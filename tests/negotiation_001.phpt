--TEST--
negotiation
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
?>
--ENV--
HTTP_ACCEPT=application/xml, application/xhtml+xml, text/html ; q = .8
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
$ctype = array(
	array('foo/bar', 'application/xhtml+xml', 'text/html')
);
var_dump(http_negotiate_language($langs[0]));
var_dump(http_negotiate_language($langs[0], $lresult));
var_dump(http_negotiate_charset($csets[0]));
var_dump(http_negotiate_charset($csets[0], $cresult));
var_dump(http_negotiate_content_type($ctype[0]));
var_dump(http_negotiate_content_type($ctype[0], $tresult));
var_dump(http_negotiate_language(array("unknown")));
var_dump(http_negotiate_charset(array("unknown")));
var_dump(http_negotiate_content_type(array("unknown")));
print_r($lresult);
print_r($cresult);
print_r($tresult);
echo "Done\n";
?>
--EXPECTF--
%sTEST
string(2) "de"
string(2) "de"
string(10) "iso-8859-1"
string(10) "iso-8859-1"
string(21) "application/xhtml+xml"
string(21) "application/xhtml+xml"
string(7) "unknown"
string(7) "unknown"
string(7) "unknown"
Array
(
    [de] => 900
    [en] => 0.27
)
Array
(
    [iso-8859-1] => 1000
    [utf-8] => 0.7
)
Array
(
    [application/xhtml+xml] => 999
    [text/html] => 0.8
)
Done
