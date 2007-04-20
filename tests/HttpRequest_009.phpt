--TEST--
HttpRequest callbacks
--SKIPIF--
<?php
include 'skip.inc';
checkcls('HttpRequest');
?>
--FILE--
<?php
echo "-TEST\n";

class _R extends HttpRequest
{
	function onProgress($progress)
	{
		print_r($progress);
	}
	
	function onFinish()
	{
		var_dump($this->getResponseCode());
	}
}

$r = new _R('http://dev.iworks.at/ext-http/.print_request.php', HTTP_METH_POST);
$r->addPostFile('upload', __FILE__, 'text/plain');
$r->send();

echo "Done\n";
?>
--EXPECTF--
%sTEST
Array
(
    [dltotal] => %f
    [dlnow] => %f
    [ultotal] => %f
    [ulnow] => %f
)
%srray
(
    [dltotal] => %f
    [dlnow] => %f
    [ultotal] => %f
    [ulnow] => %f
)
int(200)
Done
