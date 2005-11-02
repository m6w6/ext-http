--TEST--
http_redirect() with session
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5.1);
checkext('session');
?>
--ENV--
HTTP_HOST=localhost
--FILE--
<?php
include 'log.inc';
log_prepare(_REDIR_LOG);
session_start();
http_redirect('redirect', array('a' => 1), true);
?>
--EXPECTF--
Status: 302
X-Powered-By: PHP/%s
Set-Cookie: PHPSESSID=%s; path=/
Expires: %s
Cache-Control: %s
Pragma: %s
Location: http://localhost/redirect?a=1&PHPSESSID=%s
Content-type: %s
