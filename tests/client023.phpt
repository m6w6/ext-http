--TEST--
client available options and configuration
--SKIPIF--
<?php 
include "skipif.inc";
skip_client_test();
?>
--FILE--
<?php 

echo "Test\n";

$client = new http\Client;
if (($opt = $client->getOptions())) {
	var_dump($options);
}
$client->setOptions($avail = $client->getAvailableOptions());
$opt = $client->getOptions();

foreach ($avail as $k => $v) {
	if (is_array($v)) {
		$oo = $opt[$k];
		foreach ($v as $kk => $vv) {
			if (isset($vv) && $oo[$kk] !== $vv) {
				var_dump(array($kk => array($vv, $oo[$kk])));
			}
		}
	} else if (isset($v) && $opt[$k] !== $v) {
		var_dump(array($k => array($v, $opt[$k])));
	}
}
var_dump($client === $client->configure($client->getAvailableConfiguration()));

?>
===DONE===
--EXPECT--
Test
bool(true)
===DONE===
