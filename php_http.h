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

#ifndef PHP_EXT_HTTP_H
#define PHP_EXT_HTTP_H

#define PHP_EXT_HTTP_VERSION "0.3.0"

/* make compile on Win32 */
#include "php_streams.h"
#include "ext/standard/md5.h"

extern zend_module_entry http_module_entry;
#define phpext_http_ptr &http_module_entry

#ifdef ZTS
#include "TSRM.h"
#define HTTP_G(v) TSRMG(http_globals_id, zend_http_globals *, v)
#else
#define HTTP_G(v) (http_globals.v)
#endif

#ifndef ZEND_ENGINE_2
#include "php_http_build_query.h"
#endif

PHP_FUNCTION(http_date);
PHP_FUNCTION(http_absolute_uri);
PHP_FUNCTION(http_negotiate_language);
PHP_FUNCTION(http_negotiate_charset);
PHP_FUNCTION(http_redirect);
PHP_FUNCTION(http_send_status);
PHP_FUNCTION(http_send_last_modified);
PHP_FUNCTION(http_match_modified);
PHP_FUNCTION(http_match_etag);
PHP_FUNCTION(http_cache_last_modified);
PHP_FUNCTION(http_cache_etag);
PHP_FUNCTION(http_content_type);
PHP_FUNCTION(http_content_disposition);
PHP_FUNCTION(http_send_data);
PHP_FUNCTION(http_send_file);
PHP_FUNCTION(http_send_stream);
PHP_FUNCTION(http_chunked_decode);
PHP_FUNCTION(http_split_response);
PHP_FUNCTION(http_parse_header);
#ifdef HTTP_HAVE_CURL
PHP_FUNCTION(http_get);
PHP_FUNCTION(http_head);
PHP_FUNCTION(http_post_data);
PHP_FUNCTION(http_post_array);
#endif
PHP_FUNCTION(http_auth_basic);
PHP_FUNCTION(http_auth_basic_cb);

PHP_MINIT_FUNCTION(http);
PHP_RSHUTDOWN_FUNCTION(http);
PHP_MINFO_FUNCTION(http);

ZEND_BEGIN_MODULE_GLOBALS(http)
	zend_bool etag_started;
	PHP_MD5_CTX etag_md5;
	php_stream_statbuf ssb;
	char *ctype;
	char *etag;
	time_t lmod;
#ifdef HTTP_HAVE_CURL
	struct {
		struct {
			char *data;
			size_t used;
			size_t free;
		} body;
		struct {
			char *data;
			size_t used;
			size_t free;
		} hdrs;
	} curlbuf;
#endif
ZEND_END_MODULE_GLOBALS(http)

#endif	/* PHP_HTTP_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

