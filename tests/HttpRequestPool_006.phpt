--TEST--
HttpRequestPool detaching in callbacks
--SKIPIF--
<?php
include 'skip.inc';
checkcls("HttpRequestPool");
checkurl("at.php.net");
checkurl("de.php.net");
?>
--FILE--
<?php
echo "-TEST\n";
class r1 extends HttpRequest {
	function onProgress() {
		$GLOBALS['p']->detach($this);
	}
}
class r2 extends HttpRequest {
	function onFinish() {
		$GLOBALS['p']->detach($this);
	}
}
$p = new HttpRequestPool(new r1("at.php.net"), new r2("de.php.net"));
$p->send();
var_dump($p->getAttachedRequests());
echo "Done\n";
?>
--EXPECTF--
%sTEST
array(0) {
}
Done
