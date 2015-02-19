--TEST--
Bug #66891 (Unexpected HTTP 401 after NTLM authentication)
--SKIPIF--
<?php
include "skipif.inc";
?>
--GET--
dummy=1
--FILE--
<?php
header("WWW-Authenticate: none");
$r = new http\Env\Response;
$r->setResponseCode(200);
$r->send();
var_dump(http\Env::getResponseCode());
?>
--EXPECT--
int(200)