--TEST--
logging redirects
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin(5.1);
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
%sTEST
%d%d%d%d-%d%d-%d%d %d%d:%d%d:%d%d	[301-REDIRECT]	Location: http%s	<%s>
Done
