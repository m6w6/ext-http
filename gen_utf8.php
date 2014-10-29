#!/usr/bin/env php
<?php

error_reporting(E_ALL);
set_error_handler(function($c, $e, $f, $l) {
	throw new Exception("$e in $f on line $l");
});

$i18n = $argc >= 2 ? $argv[1] : "/usr/share/i18n/locales/i18n";

$f = fopen($i18n, "r");
$c = false;
$a = false;

ob_start(null, 0xffff);
while (!feof($f)) {
	$line = fgets($f);
	if (!$c && $line !== "LC_CTYPE\n") {
		continue;
	}
	$c = true;
	if ($line === "END LC_CTYPE\n") {
		break;
	}
	switch($line{0}) {
	case "%":
		if ($a) {
			printf("/* %s */\n", trim($line, "%\n/ "));
		}
		break;
	case "\n":
		if ($a) {
			break 2;
		}
		break;
	case " ":
		if ($a) {
			foreach (explode(";", trim($line, "\n/ ;")) as $ranges) {
				$range = explode("..", $ranges);
				$step = 0;
				$end = 0;
				switch (count($range)) {
				case 3:
					list($sstart, $sstep, $send) = $range;
					sscanf($sstart, "<U%X>", $start);
					sscanf($sstep, "(%d)", $step);
					sscanf($send, "<U%X>", $end);

					break;
				case 2:
					list($sstart, $send) = $range;
					$step = 1;
					sscanf($sstart, "<U%X>", $start);
					sscanf($send, "<U%X>", $end);
					break;
				case 1:
					list($sstart) = $range;
					sscanf($sstart, "<U%X>", $start);
					break;
				}
				print "\t{";
				if ($start >= 0xffff) {
					printf("0x%08X, ", $start);
					if ($end) {
						printf("0x%08X, ", $end);
					} else {
						print("         0, ");
					}
				} else {
					printf("    0x%04X, ", $start);
					if ($end) {
						printf("    0x%04X, ", $end);
					} else {
						print("         0, ");
					}
				}
				printf("%d},\n", $step);
			}
		}
		break;
	default:
		if ($a) {
			break 2;
		} elseif ($line === "alpha /\n") {
			$a = true;
		}
		break;
	}
}

file_put_contents("php_http_utf8.h",
	preg_replace('/(\/\* BEGIN::UTF8TABLE \*\/\n).*(\n\s*\/\* END::UTF8TABLE \*\/)/s', '$1'. ob_get_contents() .'$2',
		file_get_contents("php_http_utf8.h")));
