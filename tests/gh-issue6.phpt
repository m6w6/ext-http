--TEST--
url - unsafe characters
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php 

echo "Test\n";

$url = new http\Url("?__utma=1152894289.1017686999.9107388726.1439222726.1494721726.1&__utmb=115739289.1.10.1437388726&__utmc=115883619&__utmx=-&__utmz=115111289.14310476.1.1.utmcsr=google|utmccn=(organic)|utmcmd=organic|utmctr=(not%20provided)&__utmv=-&__utmk=112678937");
echo $url->query;
echo "\n";
$url = new http\Url("?id={\$id}");
echo $url->query;
echo "\n";

?>
===DONE===
--EXPECT--
Test
__utma=1152894289.1017686999.9107388726.1439222726.1494721726.1&__utmb=115739289.1.10.1437388726&__utmc=115883619&__utmx=-&__utmz=115111289.14310476.1.1.utmcsr=google|utmccn=(organic)|utmcmd=organic|utmctr=(not%20provided)&__utmv=-&__utmk=112678937
id={$id}
===DONE===
