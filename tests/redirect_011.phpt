--TEST--
http_redirect() with params
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--ENV--
HTTP_HOST=localhost
--FILE--
<?php
include 'log.inc';
log_prepare(_REDIR_LOG);
http_redirect('redirect', array('a' => 1, 'b' => 2));
?>
--EXPECTF--
Status: 302
X-Powered-By: PHP/%s
Location: http://localhost/redirect?a=1&b=2
Content-type: %s

Redirecting to <a href="http://localhost/redirect?a=1&b=2">http://localhost/redirect?a=1&b=2</a>.

