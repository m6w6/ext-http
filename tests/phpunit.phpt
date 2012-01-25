--TEST--
unit tests
--SKIPIF--
<?php
@include_once "PHPUnit/Autoload.php" or die("skip need PHPUnit in include_path");
?>
--FILE--
<?php
require_once "PHPUnit/Autoload.php";
(new PHPUnit_TextUI_Command)->run([null, __DIR__."/../phpunit"]);
?>
--EXPECTF--
PHPUnit %s by Sebastian Bergmann.

%a

Time: %s, Memory: %s

OK (%d tests, %d assertions)

