#!/usr/bin/env php
<?php
// $Id: gen_curlinfo.php 323304 2012-02-17 21:13:24Z mike $

error_reporting(0);

function failure() {
	// this is why error_get_last() should return a stdClass object
	$error = error_get_last();
	fprintf(STDERR, "FAILURE: %s\n", $error["message"]);
	exit(-1);
}

function file_re($file, $pattern, $all = true) {
	static $path;
	
	$path or $path = isset($_SERVER['argv'][1]) ? $_SERVER['argv'][1].'/include/curl/' : "/usr/local/include/curl/";
	
	if ($content = file_get_contents($path . $file)) {
		if ($all) {
			if (preg_match_all($pattern, $content, $matches, PREG_SET_ORDER)) {
				return $matches;
			}
		} else {
			if (preg_match($pattern, $content, $matches)) {
				return $matches;
			}
		}
		trigger_error("no match in $file for $pattern");
	}
	failure();
}

$ifdefs = array(
	'PRIMARY_IP' => 'PHP_HTTP_CURL_VERSION(7,19,0)',
	'APPCONNECT_TIME' => 'PHP_HTTP_CURL_VERSION(7,19,0)',
    'CONDITION_UNMET' => 'PHP_HTTP_CURL_VERSION(7,19,4)',
    'PRIMARY_PORT' => 'PHP_HTTP_CURL_VERSION(7,21,0)',
    'LOCAL_PORT' => 'PHP_HTTP_CURL_VERSION(7,21,0)',
    'LOCAL_IP' => 'PHP_HTTP_CURL_VERSION(7,21,0)',
);
$exclude = array(
    'PRIVATE', 'LASTSOCKET', 'FTP_ENTRY_PATH', 'CERTINFO',
    'RTSP_SESSION_ID', 'RTSP_CLIENT_CSEQ', 'RTSP_SERVER_CSEQ', 'RTSP_CSEQ_RECV'
);

$translate = array(
	'HTTP_CONNECTCODE' => "connect_code",
	'COOKIELIST' => 'cookies',
);

$templates = array(
'STRING' => 
'	if (CURLE_OK == curl_easy_getinfo(ch, %s, &c)) {
		add_assoc_string_ex(&array, "%s", sizeof("%2$s"), c ? c : "", 1);
	}
',
'DOUBLE' => 
'	if (CURLE_OK == curl_easy_getinfo(ch, %s, &d)) {
		add_assoc_double_ex(&array, "%s", sizeof("%2$s"), d);
	}
',
'LONG' => 
'	if (CURLE_OK == curl_easy_getinfo(ch, %s, &l)) {
		add_assoc_long_ex(&array, "%s", sizeof("%2$s"), l);
	}
',
'SLIST' =>
'	if (CURLE_OK == curl_easy_getinfo(ch, %s, &s)) {
		MAKE_STD_ZVAL(subarray);
		array_init(subarray);
		for (p = s; p; p = p->next) {
			if (p->data) {
				add_next_index_string(subarray, p->data, 1);
			}
		}
		add_assoc_zval_ex(&array, "%s", sizeof("%2$s"), subarray);
		curl_slist_free_all(s);
	}
',
);

$infos = file_re('curl.h', '/^\s*(CURLINFO_(\w+))\s*=\s*CURLINFO_(STRING|LONG|DOUBLE|SLIST)\s*\+\s*\d+\s*,?\s*$/m');

ob_start();
foreach ($infos as $info) {
	list(, $full, $short, $type) = $info;
	if (in_array($short, $exclude)) continue;
	if (isset($ifdefs[$short])) printf("#if %s\n", $ifdefs[$short]);
	printf($templates[$type], $full, strtolower((isset($translate[$short])) ? $translate[$short] : $short));
	if (isset($ifdefs[$short])) printf("#endif\n");
}

file_put_contents("php_http_curl_client.c", 
	preg_replace('/(\/\* BEGIN::CURLINFO \*\/\n).*(\n\s*\/\* END::CURLINFO \*\/)/s', '$1'. ob_get_contents() .'$2',
		file_get_contents("php_http_curl_client.c")));

?>
