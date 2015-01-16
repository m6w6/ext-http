#!/usr/bin/env php
<?php

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
    'PRIVATE', 'LASTSOCKET', 'FTP_ENTRY_PATH', 'CERTINFO', 'TLS_SESSION',
    'RTSP_SESSION_ID', 'RTSP_CLIENT_CSEQ', 'RTSP_SERVER_CSEQ', 'RTSP_CSEQ_RECV'
);

$translate = array(
	'HTTP_CONNECTCODE' => "connect_code",
	'COOKIELIST' => 'cookies',
);

$templates = array(
'STRING' => 
'	if (CURLE_OK == curl_easy_getinfo(ch, %s, &c)) {
		ZVAL_STRING(&tmp, STR_PTR(c));
		zend_hash_str_update(info, "%s", lenof("%2$s"), &tmp);
	}
',
'DOUBLE' => 
'	if (CURLE_OK == curl_easy_getinfo(ch, %s, &d)) {
		ZVAL_DOUBLE(&tmp, d);
		zend_hash_str_update(info, "%s", lenof("%2$s"), &tmp);
	}
',
'LONG' => 
'	if (CURLE_OK == curl_easy_getinfo(ch, %s, &l)) {
		ZVAL_LONG(&tmp, l);
		zend_hash_str_update(info, "%s", lenof("%2$s"), &tmp);
	}
',
'SLIST' =>
'	if (CURLE_OK == curl_easy_getinfo(ch, %s, &s)) {
		array_init(&tmp);
		for (p = s; p; p = p->next) {
			if (p->data) {
				add_next_index_string(&tmp, p->data);
			}
		}
		zend_hash_str_update(info, "%s", lenof("%2$s"), &tmp);
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

file_put_contents("php_http_client_curl.c", 
	preg_replace('/(\/\* BEGIN::CURLINFO \*\/\n).*(\n\s*\/\* END::CURLINFO \*\/)/s', '$1'. ob_get_contents() .'$2',
		file_get_contents("php_http_client_curl.c")));

?>
