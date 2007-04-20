--TEST--
urlencoded cookies
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
skipif(!http_support(HTTP_SUPPORT_REQUESTS), "need request support");
?>
--FILE--
<?php
echo "-TEST\n";

$cookies = array("name" => "val=ue");

$r = new HttpRequest("http://dev.iworks.at/ext-http/.print_request.php", HTTP_METH_GET, array("cookies" => $cookies));
$r->recordHistory = true;
$r->send();
$r->setOptions(array('encodecookies' => false));
$r->send();
echo $r->getHistory()->toString(true);

echo "Done\n";
?>
--EXPECTF--
%sTEST
GET /ext-http/.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
Cookie: name=val%3Due
HTTP/1.1 200 OK
%s

Array
(
    [name] => val=ue
)

GET /ext-http/.print_request.php HTTP/1.1
User-Agent: %s
Host: dev.iworks.at
Accept: */*
Cookie: name=val=ue;
HTTP/1.1 200 OK
%s

Array
(
    [name] => val=ue
)

Done
