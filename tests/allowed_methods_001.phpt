--TEST--
allowed methods
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
?>
--FILE--
<?php
include dirname(__FILE__).'/log.inc';
log_prepare(_AMETH_LOG);
ini_set('http.allowed_methods', 'POST');
echo "Done\n";
?>
--EXPECTF--
Status: 405
Content-type: %s
X-Powered-By: PHP/%s
Allow: POST

