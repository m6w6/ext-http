<?php
extension_loaded('http') or @dl('php_http.dll') or @dl('http.so') or die('skip ext/http not available');
strncasecmp(PHP_SAPI, 'CLI', 3) or die('skip cannot run tests with CLI');
?>
