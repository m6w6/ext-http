#!/usr/bin/env php
<?php

ini_set("log_errors", false);
ini_set("display_errors", true);

if ($argc > 1) {
	if ($argv[1] === "-") {
		$file = "php://stdin";
	} else {
		$file = $argv[1];
	}
} elseif (stdin_is_readable()) {
	$file = "php://stdin";
} else {
	$file = "./package.xml";
}

if (($xml = simplexml_load_file($file))) {
	$xml_files = xmllist($xml->contents[0]);
	$dirs = ["."];
	while ($dir = array_shift($dirs)) {
		foreach (dirlist($dir) as $file) {
			if (is_gitignored($file)) {
				continue;
			}
			if (!is_dir($file)) {
				if (!in_array($file, $xml_files)) {
					echo "Missing file $file\n";
				}
			} else {
				$base = basename($file);
				if ($base{0} !== ".") {
					array_push($dirs, $file);
				}
			}
		}
	}
}

###

function error($fmt) {
	trigger_error(call_user_func_array("sprintf", func_get_args()));
}

function stdin_is_readable() {
	$r = [STDIN]; $w = $e = [];
	return stream_select($r, $w, $e, 0);
}

function is_gitignored($file) {
	static $gitignore;
	
	if (!isset($gitignore)) {
		if (is_readable(".gitignore")) {
			$gitignore = explode("\n", `find | git check-ignore --stdin`);
		} else {
			$gitignore = false;
		}
	}
	if ($gitignore) {
		return in_array($file, $gitignore);
	}
	return false;
}

function xmllist(SimpleXmlElement $dir, $p = ".", &$a = null) {
	settype($a, "array");
	$p = trim($p, "/") . "/" . trim($dir["name"], "/") . "/";
	foreach ($dir as $file) {
		switch ($file->getName()) {
			case "dir":
				xmllist($file, $p, $a);
				break;
			case "file":
				$a[] = sprintf("%s/%s", trim($p, "/"), trim($file["name"]));
				break;
			default:
				error("Unknown content type: %s", $file->getName());
				break;
		}
	}
	return $a;
}

function dirlist($dir, $p = null) {
	$p = implode("/", array_filter([trim($p, "/"), trim($dir, "/")]));
	foreach (scandir($p) as $file) {
		yield $p."/".$file;
	}
}
