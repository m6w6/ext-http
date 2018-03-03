#!/usr/bin/env php
<?php

error_reporting(E_ALL);
set_error_handler(function($c, $e, $f, $l) {
	throw new Exception("$e in $f on line $l");
});

$i18n = $argc >= 2 ? $argv[1] : "/usr/share/i18n/locales/i18n";

$b = 10;

$f = fopen($i18n, "r");
$c = false;
$a = false;

ob_start(null, 0xfffff);
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
				$r[$start >> $b][]=[$start,$end,$step];
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

function sp($sp, $ch = " ") { return str_repeat($ch, $sp); }
printf("switch (ch >> %d) {\n", $b);
foreach ($r as $sw => $specs) {
	printf("case 0x%08X:\n", $sw);
	$sp = 1;
	foreach ($specs as list($start, $end, $step)) {
		if ($end) {
			if ($step > 1) {
				die("\nUNEXPECTED: step>1\n");
				printf("\tfor (i=0x%08X; i <= 0x%08X; i+= %d) { if (i == ch) return 1; }\n", $start, $end, $step);
			} else {
				//printf(" if (ch >= 0x%08X && ch <= 0x%08X) return 1;\n", $start, $end);
				printf("%sif (ch >= 0x%08X) {\n", sp($sp), $start);
				printf("%sif (ch <= 0x%08X) return 1;\n", sp(++$sp), $end);
			}
		} else {
			printf("%sif (ch == 0x%08X) return 1;\n", sp($sp), $start);
		}
	}
	printf(" %s\n break;\n", sp(--$sp, "}"));
}
printf("}\n");

file_put_contents("php_http_utf8.h",
	preg_replace('/(\/\* BEGIN::UTF8SWITCH \*\/\n).*(\n\s*\/\* END::UTF8SWITCH \*\/)/s', '$1'. ob_get_contents() .'$2',
		file_get_contents("php_http_utf8.h")));
