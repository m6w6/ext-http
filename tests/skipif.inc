<?php
function _ext($ext) { extension_loaded($ext) or die("skip $ext extension needed\n"); }
_ext("http");

function skip_online_test($message = "skip test requiring internet connection\n") {
	if (getenv("SKIP_ONLINE_TESTS")) {
		die($message);
	}
}

function skip_slow_test($message = "skip slow test\n") {
	if (getenv("SKIP_SLOW_TESTS")) {
		die($message);
	}
}

function skip_client_test($message = "skip need a client driver\n") {
	if (!http\Client::getAvailableDrivers()) {
		die($message);
	}
}
