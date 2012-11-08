<?php
require_once "PHPUnit/Autoload.php";
$c = new PHPUnit_TextUI_Command;
$c->run(array_merge($argv, array(__DIR__."/phpunit/")));
