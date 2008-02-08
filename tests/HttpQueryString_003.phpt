--TEST--
HttpQueryString xlate
--SKIPIF--
<?php
include 'skip.inc';
checkmin("5.2.5");
checkext('iconv');
?>
--FILE--
<?php
echo "-TEST\n";
$qs = new HttpQueryString(false, "ä[0]=ü&ö[a]=ß");
var_dump($qs->get());
$qs->xlate("latin1", "utf8");
var_dump($qs->get());
$qs->xlate("utf8", "latin1");
var_dump($qs->get());
echo "Done\n";
--EXPECTF--
%aTEST
string(29) "%E4%5B0%5D=%FC&%F6%5Ba%5D=%DF"
string(41) "%C3%A4%5B0%5D=%C3%BC&%C3%B6%5Ba%5D=%C3%9F"
string(29) "%E4%5B0%5D=%FC&%F6%5Ba%5D=%DF"
Done
