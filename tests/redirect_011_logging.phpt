--TEST--
logging redirects
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin("5.2.5");
?>
--ENV--
HTTP_HOST=example.com
--FILE--
<?php
echo "-TEST\n";
include 'log.inc';
log_content(_REDIR_LOG);
echo "Done";
?>
--EXPECTF--
%aTEST
%d%d%d%d-%d%d-%d%d %d%d:%d%d:%d%d	[302-REDIRECT]	Location: http%a	<%a>
Done
