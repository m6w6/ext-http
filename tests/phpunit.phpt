--TEST--
unit tests
--SKIPIF--
<?php
include "skipif.inc";
if (!@include_once "PHPUnit/Autoload.php") die("skip need PHPUnit in include_path");
?>
--INI--
date.timezone=Europe/Vienna
--FILE--
<?php
require_once "PHPUnit/Autoload.php";
$c = new PHPUnit_TextUI_Command;
$c->run(array("--process-isolation", __DIR__."/../phpunit/"));
?>
--EXPECTF--
PHPUnit %s by Sebastian Bergmann.

%a

Time: %s, Memory: %s

OK (%d tests, %d assertions)

