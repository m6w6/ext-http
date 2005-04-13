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
#include "php_streams.h"
#include "ext/standard/md5.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_cache_api.h"
#include "php_http_send_api.h"
#include "php_http_api.h"
#include "php_http_date_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

/* {{{ char *http_etag(void *, size_t, http_send_mode) */
PHP_HTTP_API char *_http_etag(const void *data_ptr, size_t data_len, http_send_mode data_mode TSRMLS_DC)
{
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
			if (!HTTP_G(ssb).sb.st_ino) {
				if (php_stream_stat((php_stream *) data_ptr, &HTTP_G(ssb))) {
					return NULL;
				}
			}
			snprintf(ssb_buf, 127, "%ld=%ld=%ld",
				HTTP_G(ssb).sb.st_mtime,
				HTTP_G(ssb).sb.st_ino,
				HTTP_G(ssb).sb.st_size
			);
			PHP_MD5Update(&ctx, ssb_buf, strlen(ssb_buf));
		break;

		default:
			efree(new_etag);
			return NULL;
		break;
	}

	PHP_MD5Final(digest, &ctx);
	make_digest(new_etag, digest);

	return new_etag;
}
/* }}} */

/* {{{ time_t http_lmod(void *, http_send_mode) */
PHP_HTTP_API time_t _http_lmod(const void *data_ptr, http_send_mode data_mode TSRMLS_DC)
{
	switch (data_mode)
	{
		case SEND_DATA:
		{
			return time(NULL);
		}

		case SEND_RSRC:
		{
			php_stream_stat((php_stream *) data_ptr, &HTTP_G(ssb));
			return HTTP_G(ssb).sb.st_mtime;
		}

		default:
		{
			php_stream_stat_path(Z_STRVAL_P((zval *) data_ptr), &HTTP_G(ssb));
			return HTTP_G(ssb).sb.st_mtime;
		}
	}
}
/* }}} */

/* {{{ zend_bool http_modified_match(char *, time_t) */
PHP_HTTP_API zend_bool _http_modified_match_ex(const char *entry, time_t t, zend_bool enforce_presence TSRMLS_DC)
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

/* {{{ zend_bool http_etag_match(char *, char *) */
PHP_HTTP_API zend_bool _http_etag_match_ex(const char *entry, const char *etag, zend_bool enforce_presence TSRMLS_DC)
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
	if (cc_len) {
		http_send_cache_control(cache_control, cc_len);
	}

	if (http_modified_match("HTTP_IF_MODIFIED_SINCE", last_modified)) {
		if (SUCCESS == http_send_status(304)) {
			zend_bailout();
		} else {
			http_error(E_WARNING, HTTP_E_HEADER, "Could not send 304 Not Modified");
			return FAILURE;
		}
	}
	return http_send_last_modified(send_modified);
}
/* }}} */

/* {{{ STATUS http_cache_etag(char *, size_t, char *, size_t) */
PHP_HTTP_API STATUS _http_cache_etag(const char *etag, size_t etag_len,
	const char *cache_control, size_t cc_len TSRMLS_DC)
{
	if (cc_len) {
		http_send_cache_control(cache_control, cc_len);
	}

	if (etag_len) {
		http_send_etag(etag, etag_len);
		if (http_etag_match("HTTP_IF_NONE_MATCH", etag)) {
			if (SUCCESS == http_send_status(304)) {
				zend_bailout();
			} else {
				http_error(E_WARNING, HTTP_E_HEADER, "Could not send 304 Not Modified");
				return FAILURE;
			}
		}
	}

	/* if no etag is given and we didn't already start ob_etaghandler -- start it */
	if (!HTTP_G(etag_started)) {
		if (SUCCESS == http_start_ob_handler(_http_ob_etaghandler, "ob_etaghandler", 4096, 1)) {
			HTTP_G(etag_started) = 1;
			return SUCCESS;
		} else {
			http_error(E_WARNING, HTTP_E_OBUFFER, "Could not start ob_etaghandler");
			return FAILURE;
		}
	}
	return SUCCESS;
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

