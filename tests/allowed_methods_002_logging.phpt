--TEST--
logging allowed methods
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
log_content(_AMETH_LOG);
echo "Done";
?>
--EXPECTF--
%aTEST
%d%d%d%d-%d%d-%d%d %d%d:%d%d:%d%d	[405-ALLOWED]	Allow: POST	<%a>
Done
