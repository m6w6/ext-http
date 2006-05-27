--TEST--
allowed methods
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmax(5.0);
?>
--FILE--
<?php
include 'log.inc';
log_prepare(_AMETH_LOG);
ini_set('http.request.methods.allowed', 'POST');
echo "Done\n";
?>
--EXPECTF--
Status: 405
Content-type: %s
X-Powered-By: PHP/%s
Allow: POST

