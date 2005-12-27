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
echo $i->flush($d->flush("Hi "));
echo $i->finish($d->finish("there!\n"));
echo $i->finish($d->finish("Yo...\n"));
echo "Done\n";
?>
--EXPECTF--
%sTEST
Hi there!
Yo...
Done

