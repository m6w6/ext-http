--TEST--
env response codes
--SKIPIF--
<?php include "skipif.inc"; ?>
--FILE--
<?php
echo "Test\n";

var_dump(
	array_diff_assoc(
		http\Env::getResponseStatusForAllCodes(),
		array_filter(
			array_map(
				array("http\\Env", "getResponseStatusForCode"),
				range(0, 600)
			)
		)
	)
);
?>
Done
--EXPECT--
Test
array(0) {
}
Done
