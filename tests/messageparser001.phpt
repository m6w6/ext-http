--TEST--
message parser
--SKIPIF--
<?php
include "skipif.inc";
?>
--FILE--
<?php

echo "Test\n";

use http\Message\Parser;

foreach (glob(__DIR__."/data/message_*.txt") as $file) {
	$string = "";
	$parser = new Parser;
	$fd = fopen($file, "r") or die("Could not open $file");
	while (!feof($fd)) {
		switch ($parser->parse(fgets($fd), 0, $message)) {
		case Parser::STATE_DONE:
			$string = (string) $message;
			break 2;
		case Parser::STATE_FAILURE:
			throw new Exception(($e = error_get_last()) ? $e["message"] : "Could not parse $file");
		}
	}
	
	if (!$string) {
		$s = array("START", "HEADER", "HEADER_DONE", "BODY", "BODY_DUMB", "BODY_LENGTH", "BODY_CHUNK", "BODY_DONE", "UPDATE_CL", "DONE");
		printf("Unexpected state: %s (%s)\n", $s[$parser->getState()], $file);
	}

	$parser = new Parser;
	rewind($fd);
	unset($message);

	switch ($parser->stream($fd, 0, $message)) {
	case Parser::STATE_DONE:
	case Parser::STATE_START:
		break;
	default:
		printf("Expected parser state 0 or 8, got %d", $parser->getState());
	}
	if ($string !== (string) $message) {
		$a = explode("\n", $string);
		$b = explode("\n", (string) $message);
		do {
			$aa = array_shift($a);
			$bb = array_shift($b);
			if ($aa !== $bb) {
				isset($aa) and printf("-- %s\n", $aa);
				isset($bb) and printf("++ %s\n", $bb);
			}
		} while ($a || $b);
	}
}
?>
DONE
--EXPECT--
Test
DONE
