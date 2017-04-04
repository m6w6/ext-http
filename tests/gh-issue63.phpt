--TEST--
gh issue #63: Url::mod() breaks query strings containing plus-notation spaces in the input URL
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

use http\QueryString;
use http\Url;

$url = 'http://example.com/?param=has+spaces';
$query = ['foo' => 'bar'];
$parts['query'] = new QueryString($query);
echo (new Url($url, null, Url::STDFLAGS))->mod($parts)->toString();

?>

===DONE===
--EXPECTF--
Test
http://example.com/?param=has%20spaces&foo=bar
===DONE===
