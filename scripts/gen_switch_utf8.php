#!/usr/bin/env php
<?php

error_reporting(E_ALL);
set_error_handler(function($c, $e, $f, $l) {
	throw new Exception("$e in $f on line $l");
});

$i18n = $argc >= 2 ? $argv[1] : "/usr/share/i18n/locales/i18n";

$b = 10;
$m = 0xfffff000 >> $b;
$b2 = 8;
$x = $m >> $b2;

$f = fopen($i18n, "r");
$c = false;
$a = false;

$ranges = $lables = $gotos = [];

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
			foreach (explode(";", trim($line, "\n/ ;")) as $list) {
				$range = explode("..", $list);
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
				$sw = $start >> $b;
				$sw2 = ($start & $m) >> $b2;
				if (isset($ranges[$sw][$sw2])) {
					//$ranges[$sw][$sw2] = array_filter($ranges[$sw][$sw2]);
				}
				$ranges[$sw][$sw2][]=[$start,$end,$step];
				if ($end) {
					$goto = $start;
				}
				while (($start += $step) <= $end) {
					$ssw = $start >> $b;
					$ssw2 = ($start & $m) >> $b2;
					if (!isset($ranges[$ssw][$ssw2]) || null !== end($ranges[$ssw][$ssw2])) {
						$ranges[$ssw][$ssw2][]=null;
					}
					if ($ssw != $sw || $ssw2 != $sw2) {
						$gotos[$ssw][$ssw2] = $goto;
						$labels[$sw][$sw2] = $goto;
					}
				}
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
foreach ($ranges as $sw => $sws) {
	printf("case 0x%08X:\n", $sw);
	printf(" switch((ch & 0x%08X) >> %d) {\n", $m, $b2);
	foreach ($sws as $sw2 => $specs) {
		printf(" case 0x%08X:\n", $sw2);
		$sp = 2;
		$start = null;
		foreach ($specs as $index => $spec) {
			if ($spec) {
				list($start, $end, $step) = $spec;
				if (isset($labels[$sw][$sw2])) {
					$label = $labels[$sw][$sw2];
					if ((!$end && $label == $start) || ($end && $label >= $start && $label <= $end)) {
						printf("%sc_%08X:;\n", sp($sp), $label);
					}
				}
				if ($end) {
					if ($step > 1) {
						die("\nUNEXPECTED: step>1\n");
						printf("\tfor (i=0x%08X; i <= 0x%08X; i+= %d) { if (i == ch) return 1; }\n", $start, $end, $step);
					} else {
						printf("%sif (ch >= 0x%08X) {\n", sp($sp), $start);
						printf("%sif (ch <= 0x%08X) return 1;\n", sp(++$sp), $end);
					}
				} else {
					printf("%sif (ch == 0x%08X) return 1;\n", sp($sp), $start);
				}			
			} else {
				if (isset($gotos[$sw][$sw2]) && !$start) {
					if (isset($specs[$index + 1])) {
						list($next) = $specs[$index + 1];
						printf("%sif (ch < 0x%08X)\n ", sp($sp), $next);
					}
					$goto = $gotos[$sw][$sw2];
					printf("%sgoto c_%08X;\n", sp($sp), $goto);
					$start = $goto;
				}
			}
		}
		if ($sp > 2) {
			printf("  %s\n", sp($sp-2, "}"));
		}
		printf("  break;\n");
	}
	printf(" }\n break;\n");
}
printf("}\n");

file_put_contents("php_http_utf8.h",
	preg_replace('/(\/\* BEGIN::UTF8SWITCH \*\/\n).*(\n\s*\/\* END::UTF8SWITCH \*\/)/s', '$1'. ob_get_contents() .'$2',
		file_get_contents("php_http_utf8.h")));
