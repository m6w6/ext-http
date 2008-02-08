<?php
defined('STDOUT') or define('STDOUT', fopen('php://stdout', 'w'));
if (!function_exists('fprintf')) {
	function fprintf(){ $a=func_get_args(); $s=array_shift($a); return fwrite($s, call_user_func_array('sprintf',$a)); }
}
function skipif($if, $skip) { if ($if) { fprintf(STDOUT, "skip $skip"); exit(); }}
function checkcgi() { skipif(!strncasecmp('CLI', PHP_SAPI, 3), 'need CGI SAPI'); }
function checkext($ext) { skipif(!extension_loaded($ext), "need ext/$ext"); }
function checkmin($ver) { skipif(version_compare(PHP_VERSION, $ver) < 0, sprintf("need PHP >= v%s",$ver)); }
function checkmax($ver) { skipif(version_compare(PHP_VERSION, $ver) > 0, sprintf("need PHP <= v%s",$ver)); }
function checkurl($url) { skipif(!@fsockopen($url, 80), "$url not responsive"); }
function checkcls($cls) { skipif(!class_exists($cls), "need class $cls"); }
function checkver($ver) { checkmin($ver); }
checkext('http');
?>
