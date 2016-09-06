--TEST--
crash with bad url passed to http\Message::setRequestUrl()
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php 
echo "Test\n";

$urls = array(
		"http://.foo.bar",
		"http://foo..bar",
		"http://foo.bar.",
);

foreach ($urls as $url) {
	try {
		$c = new http\Client\Request;
		$c->setRequestUrl($url);
		printf("OK: %s\n", $url);
	} catch (Exception $e) {
		printf("%s\n", $e->getMessage());
	}
}

?>
===DONE===
--EXPECT--
Test
http\Message::setRequestUrl(): Failed to parse host; unexpected '.' at pos 0 in '.foo.bar'
http\Message::setRequestUrl(): Failed to parse host; unexpected '.' at pos 4 in 'foo..bar'
OK: http://foo.bar.
===DONE===
