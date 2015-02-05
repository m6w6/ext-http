--TEST--
env response env request
--SKIPIF--
<?php
include "skipif.inc";
?>
--GET--
dummy=1
--FILE--
<?php
echo "Test\n";

$tmp = tmpfile();

// modify HTTP env
$req = new http\Env\Request;
$req->setHeader("Range", "bytes=2-4");

$res = new http\Env\Response;
$res->setEnvRequest($req);
$res->setContentType("text/plain");
$res->getBody()->append("012345679");
$res->send($tmp);

rewind($tmp);
var_dump(stream_get_contents($tmp));

?>
Done
--EXPECTF--
Test
string(%d) "HTTP/1.1 206 Partial Content%c
Accept-Ranges: bytes%c
X-Powered-By: %s%c
Content-Type: text/plain%c
Content-Range: bytes 2-4/9%c
Transfer-Encoding: chunked%c
%c
3%c
234%c
0%c
%c
"
Done
