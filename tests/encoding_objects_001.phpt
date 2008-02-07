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

$id = $i->update($d->update($pd = file_get_contents(__FILE__)));
foreach (glob('*.phpt') as $f) {
	$id .= $i->update($d->update($tmp = file_get_contents($f)));
	$pd .= $tmp;
}
$id .= $i->finish($d->finish());

var_dump($id == $pd);

echo "Done\n";
?>
--EXPECTF--
%aTEST
Hi there!
Yo...
bool(true)
Done

