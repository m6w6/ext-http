--TEST--
http_redirect() with session
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin("5.2.5");
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
Status: 302%s
X-Powered-By: PHP/%a
Set-Cookie: PHPSESSID=%a; path=/
Expires: %a
Cache-Control: %a
Pragma: %a
Location: http://localhost/redirect?a=1&PHPSESSID=%a
Content-type: %a
