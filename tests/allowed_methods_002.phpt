--TEST--
allowed methods
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin("5.2.5");
?>
--FILE--
<?php
include 'log.inc';
log_prepare(_AMETH_LOG);
ini_set('http.request.methods.allowed', 'POST');
echo "Done\n";
?>
--EXPECTF--
Status: 405%s
X-Powered-By: PHP/%a
Allow: POST
Content-type: %a

