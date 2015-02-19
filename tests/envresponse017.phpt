--TEST--
env response stream: no chunked transfer encoding for CONNECTs
--SKIPIF--
<?php 
include "skipif.inc";
?>
--FILE--
<?php 
echo "Test\n";

$req = new http\Env\Request;
$req->setRequestMethod("CONNECT");
$req->setRequestUrl(array("host"=>"www.example.com", "port"=>80));

echo $req;

$res = new http\Env\Response;
$res->setEnvRequest($req);
$res->send(STDOUT);

?>
===DONE===
--EXPECTF--
Test
CONNECT www.example.com:80 HTTP/1.1
HTTP/1.1 200 OK

===DONE===
