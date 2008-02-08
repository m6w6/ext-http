--TEST--
http_redirect() permanent
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin("5.2.5");
?>
--ENV--
HTTP_HOST=localhost
--FILE--
<?php
include 'log.inc';
log_prepare(_REDIR_LOG);
http_redirect('redirect', null, false, HTTP_REDIRECT_PERM);
?>
--EXPECTF--
Status: 301%s
X-Powered-By: PHP/%a
Location: http://localhost/redirect
Content-type: %a

Redirecting to <a href="http://localhost/redirect">http://localhost/redirect</a>.

