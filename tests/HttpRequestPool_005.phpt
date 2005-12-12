--TEST--
HttpRequestPool exception
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
checkcls('HttpRequestPool');
?>
--FILE--
<?php
echo "-TEST\n";

$p = new HttpRequestPool(new HttpRequest('http://_____'));
try {
	$p->send();
} catch (HttpRequestPoolException $x) {
	var_dump(count($x->exceptionStack));
}
$p = new HttpRequestPool(new HttpRequest('http://_____'), new HttpRequest('http://_____'));
try {
	$p->send();
} catch (HttpRequestPoolException $x) {
	var_dump(count($x->exceptionStack));
}

try {
	$p = new HttpRequestPool(new HttpRequest);
} catch (HttpRequestPoolException $x) {
	var_dump(count($x->exceptionStack));
}

try {
	$p = new HttpRequestPool(new HttpRequest, new HttpRequest);
} catch (HttpRequestPoolException $x) {
	var_dump(count($x->exceptionStack));
}

echo "Done\n";
?>
--EXPECTF--
%sTEST
int(2)
int(4)
int(1)
int(2)
Done

