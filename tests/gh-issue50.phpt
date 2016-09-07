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

$c = new http\Client;
$c->enqueue(new http\Client\Request("GET", "http://example.com"));

$c->setDebug(function(http\Client $c, http\Client\Request $r, $type, $data) {
    if ($type & http\Client::DEBUG_HEADER) {
        $c->dequeue($r);
    }
});

try {
	$c->send();
} catch (Exception $e) {
	echo $e;
}

?>

===DONE===
--EXPECTF--
Test
http\Exception\RuntimeException: http\Client::dequeue(): Could not dequeue request while executing callbacks in %sgh-issue50.php:9
Stack trace:
#0 %sgh-issue50.php(9): http\Client->dequeue(Object(http\Client\Request))
#1 [internal function]: {closure}(Object(http\Client), Object(http\Client\Request), 18, 'GET / HTTP/1.1%s...')
#2 %sgh-issue50.php(14): http\Client->send()
#3 {main}
===DONE===
