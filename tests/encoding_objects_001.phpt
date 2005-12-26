--TEST--
encoding stream objects
--SKIPIF--
<?php
include 'skip.inc';
checkver(5);
skipif(!http_support(HTTP_SUPPORT_ENCODINGS), "need zlib");
?>
--FILE--
<?php
echo "-TEST\n";
$d = new HttpDeflateStream;
$i = new HttpInflateStream;
echo $i->update($d->update("Hi "));
echo $i->update($d->update("there!\n"));
echo $i->update($d->finish());
echo $i->finish();
echo "Done\n";
?>
--EXPECTF--
%sTEST
Hi there!
Done

