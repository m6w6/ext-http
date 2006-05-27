--TEST--
allowed methods
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin(5.1);
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
X-Powered-By: PHP/%s
Allow: POST
Content-type: %s

