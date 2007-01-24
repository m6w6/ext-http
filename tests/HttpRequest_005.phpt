--TEST--
HttpRequest accessors
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpRequest');
?>
--FILE--
<?php
echo "-TEST\n";
error_reporting(0);
$r = new HttpRequest;
foreach (get_class_methods('HttpRequest') as $method) {
	try {
	 	if (strlen($method) > 3 && substr($method, 0, 3) == 'get')
			$x = $r->$method();
	} catch (HttpException $e) {
	}
}
echo "Done\n";
?>
--EXPECTF--
%sTEST
Done
