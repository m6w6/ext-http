<?php
_ext("http");

function _ext($ext) { 
	if (!extension_loaded($ext)) {
		die("skip $ext extension needed\n");
	}
}

function utf8locale() {
	if (stristr(setlocale(LC_CTYPE, "C.UTF-8"), "utf")) {
		return true;
	}
	$locale = setlocale(LC_CTYPE, null);
	if (stristr($locale, "utf") && substr($locale, -1) === "8") {
		return true;
	}
	return false;
}

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

function skip_http2_test($message = "skip need http2 support") {
	if (!defined("http\\Client\\Curl\\HTTP_VERSION_2_0")) {
		die("$message (HTTP_VERSION_2_0)\n");
	}
	if (!(http\Client\Curl\FEATURES & http\Client\Curl\Features\HTTP2)) {
		die("$message (FEATURES & HTTP2)\n");
	}
	foreach (explode(":", $_ENV["PATH"]) as $path) {
		if (is_executable($path . "/nghttpd")) {
			return;
		}
	}
	die("$message (nghttpd in PATH)\n");
}
