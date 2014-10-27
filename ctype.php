<?php

error_reporting(E_ALL);
set_error_handler(function($c, $e, $f, $l) {
	throw new Exception("$e in $f on line $l");
});

$i18n = $argc >= 2 ? $argv[1] : "/usr/share/i18n/locales/i18n";

$f = fopen($i18n, "r");
$c = false;
$a = false;
$r = array();

print <<<C
typedef struct utf8_range {
	unsigned int start;
	unsigned int end;
	unsigned char step;
} utf8_range_t;

static const utf8_range_t utf8_ranges[] = {

C;
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

print <<<C
	{0, 0, 0}
};

C;
