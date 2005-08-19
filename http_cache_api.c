/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "php.h"

#include "SAPI.h"
#include "php_streams.h"
#include "php_output.h"
#include "ext/standard/md5.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_cache_api.h"
#include "php_http_send_api.h"
#include "php_http_date_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

/* {{{ STATUS http_cache_exit(char *, zend_bool) */
STATUS _http_cache_exit_ex(char *cache_token, zend_bool etag, zend_bool free_token TSRMLS_DC)
{
	if (HTTP_G(log).cache && strlen(HTTP_G(log).cache)) {
		php_stream *log = php_stream_open_wrapper(HTTP_G(log).cache, "ab", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL);

		if (log) {
			time_t now;
			struct tm nowtm;
			char datetime[128];

			time(&now);
			strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", php_localtime_r(&now, &nowtm));
 			php_stream_printf(log TSRMLS_CC, "%s [%s] %32s %s\n", datetime, etag ? "ETAG":"LMOD", cache_token, SG(request_info).request_uri);
 			php_stream_close(log);
		}
	}
	if (free_token && cache_token) {
		efree(cache_token);
	}
	return http_exit_ex(304, NULL, 0);
}
/* }}} */

/* {{{ char *http_etag(void *, size_t, http_send_mode) */
PHP_HTTP_API char *_http_etag(const void *data_ptr, size_t data_len, http_send_mode data_mode TSRMLS_DC)
{
	php_stream_statbuf ssb;
	char ssb_buf[128] = {0};
	unsigned char digest[16];
	PHP_MD5_CTX ctx;
	char *new_etag = ecalloc(1, 33);

	PHP_MD5Init(&ctx);

	switch (data_mode)
	{
		case SEND_DATA:
			PHP_MD5Update(&ctx, data_ptr, data_len);
		break;

		case SEND_RSRC:
		{
			if (php_stream_stat((php_stream *) data_ptr, &ssb)) {
				efree(new_etag);
				return NULL;
			}

			snprintf(ssb_buf, 127, "%ld=%ld=%ld", ssb.sb.st_mtime, ssb.sb.st_ino, ssb.sb.st_size);
			PHP_MD5Update(&ctx, ssb_buf, strlen(ssb_buf));
		}
		break;

		default:
		{
			if (php_stream_stat_path((char *) data_ptr, &ssb)) {
				efree(new_etag);
				return NULL;
			}

			snprintf(ssb_buf, 127, "%ld=%ld=%ld", ssb.sb.st_mtime, ssb.sb.st_ino, ssb.sb.st_size);
			PHP_MD5Update(&ctx, ssb_buf, strlen(ssb_buf));
		}
		break;
	}

	PHP_MD5Final(digest, &ctx);
	make_digest(new_etag, digest);

	return new_etag;
}
/* }}} */

/* {{{ time_t http_last_modified(void *, http_send_mode) */
PHP_HTTP_API time_t _http_last_modified(const void *data_ptr, http_send_mode data_mode TSRMLS_DC)
{
	php_stream_statbuf ssb;

	switch (data_mode)
	{
		case SEND_DATA:	return time(NULL);
		case SEND_RSRC:	return php_stream_stat((php_stream *) data_ptr, &ssb) ? 0 : ssb.sb.st_mtime;
		default:		return php_stream_stat_path((char *) data_ptr, &ssb) ? 0 : ssb.sb.st_mtime;
	}
}
/* }}} */

/* {{{ zend_bool http_match_last_modified(char *, time_t) */
PHP_HTTP_API zend_bool _http_match_last_modified_ex(const char *entry, time_t t, zend_bool enforce_presence TSRMLS_DC)
{
	zend_bool retval;
	zval *zmodified;
	char *modified, *chr_ptr;

	HTTP_GSC(zmodified, entry, !enforce_presence);

	modified = estrndup(Z_STRVAL_P(zmodified), Z_STRLEN_P(zmodified));
	if (chr_ptr = strrchr(modified, ';')) {
		chr_ptr = 0;
	}
	retval = (t <= http_parse_date(modified));
	efree(modified);
	return retval;
}
/* }}} */

/* {{{ zend_bool http_match_etag(char *, char *) */
PHP_HTTP_API zend_bool _http_match_etag_ex(const char *entry, const char *etag, zend_bool enforce_presence TSRMLS_DC)
{
	zval *zetag;
	char *quoted_etag;
	zend_bool result;

	HTTP_GSC(zetag, entry, !enforce_presence);

	if (NULL != strchr(Z_STRVAL_P(zetag), '*')) {
		return 1;
	}

	quoted_etag = (char *) emalloc(strlen(etag) + 3);
	sprintf(quoted_etag, "\"%s\"", etag);

	if (!strchr(Z_STRVAL_P(zetag), ',')) {
		result = !strcmp(Z_STRVAL_P(zetag), quoted_etag);
	} else {
		result = (NULL != strstr(Z_STRVAL_P(zetag), quoted_etag));
	}
	efree(quoted_etag);
	return result;
}
/* }}} */

/* {{{ STATUS http_cache_last_modified(time_t, time_t, char *, size_t) */
PHP_HTTP_API STATUS _http_cache_last_modified(time_t last_modified,
	time_t send_modified, const char *cache_control, size_t cc_len TSRMLS_DC)
{
	if (cc_len && (SUCCESS != http_send_cache_control(cache_control, cc_len))) {
		return FAILURE;
	}

	if (SUCCESS != http_send_last_modified(send_modified)) {
		return FAILURE;
	}

	if (http_match_last_modified("HTTP_IF_MODIFIED_SINCE", last_modified)) {
		return http_cache_exit(http_date(last_modified), 0);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_cache_etag(char *, size_t, char *, size_t) */
PHP_HTTP_API STATUS _http_cache_etag(const char *etag, size_t etag_len,
	const char *cache_control, size_t cc_len TSRMLS_DC)
{
	if (cc_len && (SUCCESS != http_send_cache_control(cache_control, cc_len))) {
		return FAILURE;
	}

	if (etag_len) {
		if (SUCCESS != http_send_etag(etag, etag_len)) {
			return FAILURE;
		}
		if (!http_match_etag("HTTP_IF_NONE_MATCH", etag)) {
			return SUCCESS;
		}
		return http_cache_exit_ex((char *)etag, 1, 0);
	}

	/* if no etag is given and we didn't already start ob_etaghandler -- start it */
	if (HTTP_G(etag).started) {
		return SUCCESS;
	}

	if (HTTP_G(etag).started = (SUCCESS == php_start_ob_buffer_named("ob_etaghandler", HTTP_SENDBUF_SIZE, 1 TSRMLS_CC))) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}
/* }}} */

/* {{{ void http_ob_etaghandler(char *, uint, char **, uint *, int) */
PHP_HTTP_API void _http_ob_etaghandler(char *output, uint output_len,
	char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
	char etag[33] = { 0 };
	unsigned char digest[16];

	if (mode & PHP_OUTPUT_HANDLER_START) {
		HTTP_G(etag).started = 1;
		PHP_MD5Init(&HTTP_G(etag).md5ctx);
	}

	PHP_MD5Update(&HTTP_G(etag).md5ctx, output, output_len);

	if (mode & PHP_OUTPUT_HANDLER_END) {
		PHP_MD5Final(digest, &HTTP_G(etag).md5ctx);

		/* just do that if desired */
		if (HTTP_G(etag).started) {
			make_digest(etag, digest);
			http_send_cache_control(HTTP_DEFAULT_CACHECONTROL, lenof(HTTP_DEFAULT_CACHECONTROL));
			http_send_etag(etag, 32);

			if (http_match_etag("HTTP_IF_NONE_MATCH", etag)) {
				http_cache_exit_ex(etag, 1, 0);
			}
		}
	}

	*handled_output_len = output_len;
	*handled_output = estrndup(output, output_len);
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */

