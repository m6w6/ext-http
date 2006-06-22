<?php
define('_REDIR_LOG', '__r_log');
define('_CACHE_LOG', '__c_log');
define('_AMETH_LOG', '__m_log');
define('_CMPST_LOG', '__a_log');

function log_prepare($log)
{
	is_file($log) and @unlink($log);
	switch ($log)
	{
		case _REDIR_LOG:	ini_set('http.log.redirect', _REDIR_LOG);			break;
		case _CACHE_LOG:	ini_set('http.log.cache', _CACHE_LOG);				break;
		case _AMETH_LOG:	ini_set('http.log.allowed_methods', _AMETH_LOG);	break;
		case _CMPTS_LOG:	ini_set('http.log.composite', _CMPST_LOG);			break;
	}
}
function log_content($log)
{
	echo file_get_contents($log);
	unlink($log);
}
?>
