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
	for ($i=0; $x; ++$i, $x = @$x->innerException) {
		printf("%s%s: %s\n", str_repeat("\t", $i), get_class($x), $x->getMessage());
	}
	var_dump($i);
}
$p = new HttpRequestPool(new HttpRequest('http://_____'), new HttpRequest('http://_____'));
try {
	$p->send();
} catch (HttpRequestPoolException $x) {
	for ($i=0; $x; ++$i, $x = @$x->innerException) {
		printf("%s%s: %s\n", str_repeat("\t", $i), get_class($x),  $x->getMessage());
	}
	var_dump($i);
}
echo "Done\n";
?>
--EXPECTF--
%sTEST
HttpRequestPoolException: Exception caused by 2 inner exception(s)
	HttpInvalidParamException: Empty or too short HTTP message: ''
		HttpRequestException: couldn't resolve host name; %s (http://_____/)
int(3)
HttpRequestPoolException: Exception caused by 4 inner exception(s)
	HttpInvalidParamException: Empty or too short HTTP message: ''
		HttpRequestException: couldn't resolve host name; %s (http://_____/)
			HttpInvalidParamException: Empty or too short HTTP message: ''
				HttpRequestException: couldn't resolve host name; %s (http://_____/)
int(5)
Done

