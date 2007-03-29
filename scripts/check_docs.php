#!/usr/bin/env php
<?php
// $Id$

function re($file, $re, &$m=NULL) {
	global $doc;
	return file_exists("$doc/$file") ? preg_match($re, file_get_contents("$doc/$file"), $m) : 0;
}
function fe($file) {
	global $doc;
	return file_exists("$doc/$file");
}
function fg($path, &$g=NULL) {
	global $doc;
	return count($g = glob("$doc/$path"));
}

$doc = "/home/mike/cvs/phpdoc/en/reference/http";
$ext = new ReflectionExtension("http");

printf("Undocumented INI options:\n");
foreach ($ext->getINIEntries() as $name => $tmp) {
	re("configuration.xml", "#<entry>$name</entry>#") or printf("\t%s (%s)\n", $name, $tmp);
}
printf("\n");

printf("Undocumented constants:\n");
foreach ($ext->getConstants() as $name => $tmp) {
	re("constants.xml", "#<constant>$name</constant>#") or printf("\t%s (%s)\n", $name, $tmp);
}
printf("\n");


printf("Undocumented functions:\n");
foreach ($ext->getFunctions() as $func) {
	/* @var $func ReflectionFunction */
	fg(sprintf("functions/*/%s.xml", strtr($func->getName(),'_','-'))) or printf("\t%s()\n", $func->getName());
}
printf("\n");

printf("Undocumented classes/members:\n");
foreach ($ext->getClasses() as $class) {
	 if (substr($class->getName(), -strlen("Exception")) === "Exception") continue;
	/* @var $class ReflectionClass */
	fg(sprintf("%s.xml", $class->getName())) or printf(" %s\n", $class->getName());
	foreach ($class->getConstants() as $name => $tmp) {
		re($class->getName().".xml", "#>$name<#") or printf("\t%s::%s (%s)\n", $class->getName(), $name, $tmp);
	}
	foreach ($class->getProperties() as $prop) {
		// blatant guess
		re($class->getName().".xml", "#>{$prop->getName()}<#") or printf("\t%s::$%s\n", $class->getName(), $prop->getName());
	}
	foreach ($class->getMethods() as $meth) {
		/* @var $meth ReflectionMethod */
		fg(sprintf("%s/%s.xml", $class->getName(), strtr(trim($meth->getName(),'_'),'_','-'))) or printf("\t%s::%s()\n", $class->getName(), $meth->getName());
	}
	printf("\n");
}
printf("\n");

?>
