--TEST--
negotiate
--SKIPIF--
<?php include "skipif.inc"; ?>
--ENV--
HTTP_ACCEPT=text/html,text/plain,text/xml;q=0.1,image/*;q=0.1,*/*;q=0
HTTP_ACCEPT_CHARSET=utf-8,iso-8859-1;q=0.8,iso-8859-15;q=0
HTTP_ACCEPT_ENCODING=gzip,deflate;q=0
HTTP_ACCEPT_LANGUAGE=de-DE,de-AT;q=0.9,en;q=0.8,fr;q=0
--FILE--
CONTENT TYPE

<?php
$ct = http\Env::negotiateContentType(array("text/html","text/xml","text/json"), $ctr);
echo "$ct: "; print_r($ctr);
$ct = http\Env::negotiateContentType(array("text/xml","text/json"), $ctr);
echo "$ct: "; print_r($ctr);
$ct = http\Env::negotiateContentType(array("text/json"), $ctr);
echo "$ct: "; print_r($ctr);
?>

CHARSET

<?php
$cs = http\Env::negotiateCharset(array("utf-8", "iso-8859-1", "iso-8859-15"), $csr);
echo "$cs: "; print_r($csr);
$cs = http\Env::negotiateCharset(array("iso-8859-1", "iso-8859-15"), $csr);
echo "$cs: "; print_r($csr);
$cs = http\Env::negotiateCharset(array("utf-16", "iso-8859-15", "iso-8859-2"), $csr);
echo "$cs: "; print_r($csr);
?>

ENCODING

<?php
$ce = http\Env::negotiateEncoding(array("gzip", "deflate", "sdch"), $cer);
echo "$ce: "; print_r($cer);
$ce = http\Env::negotiateEncoding(array("", "sdch"), $cer);
echo "$ce: "; print_r($cer);
?>

LANGUAGE

<?php
$ln = http\Env::negotiateLanguage(array("de", "en", "fr"), $lnr);
echo "$ln: "; print_r($lnr);
$ln = http\Env::negotiateLanguage(array("de-DE", "de-AT", "en"), $lnr);
echo "$ln: "; print_r($lnr);
$ln = http\Env::negotiateLanguage(array("nl", "fr", "en"), $lnr);
echo "$ln: "; print_r($lnr);
?>

CUSTOM

<?
$cc = http\Env::negotiate("a, a.b;q=0.9, c.d;q=0, *.* ; q=0.1",
    array("a.x", "c.d", "c.e", "a.b"), ".", $ccr);
echo "$cc: "; print_r($ccr);
?>
DONE
--EXPECT--
CONTENT TYPE

text/html: Array
(
    [text/html] => 0.99
    [text/xml] => 0.1
)
text/xml: Array
(
    [text/xml] => 0.1
)
text/json: Array
(
)

CHARSET

utf-8: Array
(
    [utf-8] => 0.99
    [iso-8859-1] => 0.8
)
iso-8859-1: Array
(
    [iso-8859-1] => 0.8
)
utf-16: Array
(
)

ENCODING

gzip: Array
(
    [gzip] => 0.99
)
: Array
(
)

LANGUAGE

de: Array
(
    [de] => 0.99
    [en] => 0.8
)
de-DE: Array
(
    [de-DE] => 0.99
    [de-AT] => 0.9
    [en] => 0.8
)
en: Array
(
    [en] => 0.8
)

CUSTOM

a.b: Array
(
    [a.b] => 0.9
    [c.e] => 0.1
    [a.x] => 0.1
)
DONE
