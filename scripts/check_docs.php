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


$ext = "http";
$doc = "/home/mike/cvs/phpdoc/en/reference/$ext";
$ref = new ReflectionExtension($ext);

$ini = $ref->getINIEntries();
printf("Undocumented INI options (%d):\n", count($ini));
foreach ($ini as $name => $tmp) {
	re("configuration.xml", "#<entry>$name</entry>#") or printf("\t%s (%s)\n", $name, $tmp);
}
printf("\n");

$flt = preg_grep("/^$ext\./", stream_get_filters());
printf("Undocumented stream filters (%d):\n", count($flt));
foreach ($flt as $filter) {
	fe(sprintf("streamfilters/%s.xml", substr($filter, 5))) or printf("\t%s\n", $filter);
}
printf("\n");

$con = $ref->getConstants();
printf("Undocumented constants (%d):\n", count($con));
foreach ($con as $name => $tmp) {
	re("constants.xml", "#<constant>$name</constant>#") or printf("\t%s (%s)\n", $name, $tmp);
}
printf("\n");


printf("Undocumented functions:\n");
foreach ($ref->getFunctions() as $func) {
	/* @var $func ReflectionFunction */
	fg(sprintf("functions/*/%s.xml", strtr($func->getName(),'_','-'))) or printf("\t%s()\n", $func->getName());
}
printf("\n");

printf("Undocumented classes/members:\n");
foreach ($ref->getClasses() as $class) {
	 if (substr($class->getName(), -strlen("Exception")) === "Exception") continue;
	/* @var $class ReflectionClass */
	fg(sprintf("%s.xml", $class->getName())) or printf(" %s\n", $class->getName());
	foreach ($class->getConstants() as $name => $tmp) {
		re($class->getName().".xml", "#>$name<#") or printf("\t%s::%s (%s)\n", $class->getName(), $name, $tmp);
	}
	foreach ($class->getProperties() as $prop) {
		/* @var $prop ReflectionProperty */
		$prop->isPrivate() or re($class->getName().".xml", "#>{$prop->getName()}<#") or printf("\t%s::$%s\n", $class->getName(), $prop->getName());
	}
	foreach ($class->getMethods() as $meth) {
		/* @var $meth ReflectionMethod */
		try {
			$meth->getPrototype();
		} catch (Exception $ex) {
			// if getPrototype throws an exception it's definitely not a method declared in an interface
			$meth->isPrivate() or fg(sprintf("%s/%s.xml", $class->getName(), strtr(trim($meth->getName(),'_'),'_','-'))) or printf("\t%s::%s()\n", $class->getName(), $meth->getName());
		}
		
	}
	printf("\n");
}
printf("\n");

?>
