--TEST--
HttpMessage::toMessageTypeObject()
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
checkcls('HttpRequest');
?>
--FILE--
<?php
echo "-TEST\n";

$b = HttpRequest::encodeBody(array("a"=>"b",1=>2),null);

$m = new HttpMessage;
$m->setType(HttpMessage::TYPE_REQUEST);
$m->setRequestMethod('POST');
$m->setRequestUrl("http://www.example.com");
$m->setHttpVersion('1.1');
$m->addHeaders(
	array(
		"Content-Type"	=> "application/x-www-form-urlencoded",
		"Host"			=> "www.example.com",
		"Content-Length"=> strlen($b),
	)
);
$m->setBody($b);
$r = $m->toMessageTypeObject();
echo $m,"\n";
echo "Done\n";
?>
--EXPECTF--
%aTEST
POST http://www.example.com HTTP/1.1
Content-Type: application/x-www-form-urlencoded
Host: www.example.com
Content-Length: 7

a=b&1=2

Done
