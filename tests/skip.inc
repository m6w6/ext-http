<?php 
extension_loaded('http') or die('skip ext/http not available');
strncasecmp(PHP_SAPI, 'CLI', 3) or die('skip cannot run tests with CLI');
?>