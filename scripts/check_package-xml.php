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
			if (is_commonly_ignored($file)) {
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
	foreach ($xml_files as $file) {
		if (!file_exists($file)) {
			echo "Extraneous file $file\n";
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
	static $gitignore, $gitmodules;

	if (!isset($gitmodules)) {
		if (is_readable("./.gitmodules")) {
			$gitmodules = explode("\n", `git submodule status | awk '{printf$2}'`);
		} else {
			$gitmodules = false;
		}
	}
	if (!isset($gitignore)) {
		if (is_readable("./.gitignore")) {
			$ignore_submodules = $gitmodules ? " ! -path './".implode("/*' ! -path './", $gitmodules)."/*'" : "";
			$gitignore = explode("\n", `find . $ignore_submodules | git check-ignore --stdin`);
		} else {
			$gitignore = false;
		}
	}
	if ($gitignore) {
		if (in_array($file, $gitignore)) {
			return true;
		}
	}
	if ($gitmodules) {
		foreach ($gitmodules as $module) {
			if (fnmatch("./$module/*", $file)) {
				return true;
			}
		}
	}
	return false;
}

function is_commonly_ignored($file) {
	return fnmatch("./.git*", $file)
		|| in_array($file, ["./package.xml", "./package2.xml", "./.travis.yml", "./.editorconfig"], true);
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
