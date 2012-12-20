--TEST--
response content disposition
--SKIPIF--
<?php
include "skipif.inc";
?>
--GET--
dummy=1
--FILE--
<?php

$r = new http\Env\Response;
$r->setContentDisposition(array("attachment"=>array("filename"=>basename(__FILE__))));
$r->setBody(new http\Message\Body(fopen(__FILE__,"r")));
$r->send();

?>
--EXPECTHEADERS--
Content-Disposition: attachment;filename=response012.php
--EXPECT--
<?php

$r = new http\Env\Response;
$r->setContentDisposition(array("attachment"=>array("filename"=>basename(__FILE__))));
$r->setBody(new http\Message\Body(fopen(__FILE__,"r")));
$r->send();

?>

