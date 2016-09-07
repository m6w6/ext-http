--TEST--
Null pointer deref in sanitize_value
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php
echo "Test\n";

$urls = array(
    "",
    "? = ="
);

$url0=new http\Url($urls[0], null, http\Url::FROM_ENV);
$url1=$url0->mod($urls[1]);

echo $url1;

?>

===DONE===
--EXPECTF--
Test
http://%s/
===DONE===
