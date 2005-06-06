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

#define HTTP_PEXT_VERSION "0.8.0"

/* make compile on Win32 */
#ifdef HTTP_HAVE_CURL
#	ifdef PHP_WIN32
#		include <winsock2.h>
#	endif
#	include <curl/curl.h>
#endif
#include "ext/standard/md5.h"
#include "phpstr/phpstr.h"

extern zend_module_entry http_module_entry;
#define phpext_http_ptr &http_module_entry

extern int http_module_number;

ZEND_BEGIN_MODULE_GLOBALS(http)

	struct _http_globals_etag {
		zend_bool started;
		PHP_MD5_CTX md5ctx;
	} etag;

	struct _http_globals_log {
		char *cache;
	} log;

	struct _http_globals_send {
		double throttle_delay;
		size_t buffer_size;
		char *content_type;
		char *unquoted_etag;
		time_t last_modified;
	} send;

	struct _http_globals_request {
		struct _http_globals_request_methods {
			char *allowed;
			HashTable custom;
		} methods;

#ifdef HTTP_HAVE_CURL
		struct _http_globals_request_curl {
			zend_llist copies;
#	if LIBCURL_VERSION_NUM < 0x070c00
			char error[CURL_ERROR_SIZE + 1];
#	endif
		} curl;
#endif /* HTTP_HAVE_CURL */
	} request;

ZEND_END_MODULE_GLOBALS(http)

#ifdef ZTS
#	include "TSRM.h"
#	define HTTP_G(v) TSRMG(http_globals_id, zend_http_globals *, v)
#	define HTTP_GLOBALS ((zend_http_globals *) (*((void ***) tsrm_ls))[TSRM_UNSHUFFLE_RSRC_ID(http_globals_id)])
#else
#	define HTTP_G(v) (http_globals.v)
#	define HTTP_GLOBALS http_globals
#endif
#define getGlobals(G) zend_http_globals *G = HTTP_GLOBALS;

PHP_FUNCTION(http_test);
PHP_FUNCTION(http_date);
PHP_FUNCTION(http_absolute_uri);
PHP_FUNCTION(http_negotiate_language);
PHP_FUNCTION(http_negotiate_charset);
PHP_FUNCTION(http_redirect);
PHP_FUNCTION(http_throttle);
PHP_FUNCTION(http_send_status);
PHP_FUNCTION(http_send_last_modified);
PHP_FUNCTION(http_send_content_type);
PHP_FUNCTION(http_send_content_disposition);
PHP_FUNCTION(http_match_modified);
PHP_FUNCTION(http_match_etag);
PHP_FUNCTION(http_cache_last_modified);
PHP_FUNCTION(http_cache_etag);
PHP_FUNCTION(http_send_data);
PHP_FUNCTION(http_send_file);
PHP_FUNCTION(http_send_stream);
PHP_FUNCTION(http_chunked_decode);
PHP_FUNCTION(http_split_response);
PHP_FUNCTION(http_parse_headers);
PHP_FUNCTION(http_get_request_headers);
PHP_FUNCTION(http_match_request_header);
#ifdef HTTP_HAVE_CURL
PHP_FUNCTION(http_get);
PHP_FUNCTION(http_head);
PHP_FUNCTION(http_post_data);
PHP_FUNCTION(http_post_fields);
PHP_FUNCTION(http_put_file);
PHP_FUNCTION(http_put_stream);
/*PHP_FUNCTION(http_request)*/
PHP_FUNCTION(http_request_method_register);
PHP_FUNCTION(http_request_method_unregister);
PHP_FUNCTION(http_request_method_exists);
PHP_FUNCTION(http_request_method_name);
#endif /* HTTP_HAVE_CURL */
PHP_FUNCTION(http_auth_basic);
PHP_FUNCTION(http_auth_basic_cb);
#ifndef ZEND_ENGINE_2
PHP_FUNCTION(http_build_query);
#endif /* ZEND_ENGINE_2 */
PHP_FUNCTION(ob_etaghandler);

PHP_MINIT_FUNCTION(http);
PHP_MSHUTDOWN_FUNCTION(http);
PHP_RINIT_FUNCTION(http);
PHP_RSHUTDOWN_FUNCTION(http);
PHP_MINFO_FUNCTION(http);

#endif	/* PHP_HTTP_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

