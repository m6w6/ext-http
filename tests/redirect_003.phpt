--TEST--
http_redirect() permanent
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
?>
--FILE--
<?php
include 'log.inc';
log_prepare(_REDIR_LOG);
http_redirect('redirect', null, false, true);
?>
--EXPECTF--
Status: 301
Content-type: text/html
X-Powered-By: PHP/%s
Location: http://localhost/redirect

Redirecting to <a href="http://localhost/redirect">http://localhost/redirect</a>.

