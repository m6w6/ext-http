--TEST--
segfault with Client::setDebug and Client::dequeue()
--SKIPIF--
<?php
include "skipif.inc";
skip_client_test();
skip_online_test();
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
