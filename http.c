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

#ifdef HTTP_HAVE_CURL
#	ifdef PHP_WIN32
#		include <winsock2.h>
#	endif
#	include <curl/curl.h>
#endif

#include <ctype.h>

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"

#include "SAPI.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_send_api.h"

#ifdef ZEND_ENGINE_2
#	include "php_http_util_object.h"
#	include "php_http_message_object.h"
#	include "php_http_response_object.h"
#	ifdef HTTP_HAVE_CURL
#		include "php_http_request_object.h"
#	endif
#	include "php_http_exception_object.h"
#endif

#include "phpstr/phpstr.h"

#ifdef HTTP_HAVE_CURL
#ifdef ZEND_ENGINE_2
static
ZEND_BEGIN_ARG_INFO(http_request_info_ref_3, 0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO();

static
ZEND_BEGIN_ARG_INFO(http_request_info_ref_4, 0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO();
#else
static unsigned char http_request_info_ref_3[] = {3, BYREF_NONE, BYREF_NONE, BYREF_FORCE};
static unsigned char http_request_info_ref_4[] = {4, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_FORCE};
#endif /* ZEND_ENGINE_2 */
#endif /* HTTP_HAVE_CURL */

ZEND_DECLARE_MODULE_GLOBALS(http)

#ifdef COMPILE_DL_HTTP
ZEND_GET_MODULE(http)
#endif

/* {{{ http_functions[] */
function_entry http_functions[] = {
	PHP_FE(http_test, NULL)
	PHP_FE(http_date, NULL)
	PHP_FE(http_absolute_uri, NULL)
	PHP_FE(http_negotiate_language, NULL)
	PHP_FE(http_negotiate_charset, NULL)
	PHP_FE(http_redirect, NULL)
	PHP_FE(http_send_status, NULL)
	PHP_FE(http_send_last_modified, NULL)
	PHP_FE(http_send_content_type, NULL)
	PHP_FE(http_send_content_disposition, NULL)
	PHP_FE(http_match_modified, NULL)
	PHP_FE(http_match_etag, NULL)
	PHP_FE(http_cache_last_modified, NULL)
	PHP_FE(http_cache_etag, NULL)
	PHP_FE(http_send_data, NULL)
	PHP_FE(http_send_file, NULL)
	PHP_FE(http_send_stream, NULL)
	PHP_FE(http_chunked_decode, NULL)
	PHP_FE(http_split_response, NULL)
	PHP_FE(http_parse_headers, NULL)
	PHP_FE(http_get_request_headers, NULL)
#ifdef HTTP_HAVE_CURL
	PHP_FE(http_get, http_request_info_ref_3)
	PHP_FE(http_head, http_request_info_ref_3)
	PHP_FE(http_post_data, http_request_info_ref_4)
	PHP_FE(http_post_array, http_request_info_ref_4)
#endif
	PHP_FE(http_auth_basic, NULL)
	PHP_FE(http_auth_basic_cb, NULL)
#ifndef ZEND_ENGINE_2
	PHP_FE(http_build_query, NULL)
#endif
	PHP_FE(ob_etaghandler, NULL)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ http_module_entry */
zend_module_entry http_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"http",
	http_functions,
	PHP_MINIT(http),
	PHP_MSHUTDOWN(http),
	PHP_RINIT(http),
	PHP_RSHUTDOWN(http),
	PHP_MINFO(http),
#if ZEND_MODULE_API_NO >= 20010901
	HTTP_PEXT_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */


static void free_to_free(void *s)
{
	efree(*(char **)s);
}

/* {{{ void http_globals_ctor(zend_http_globals *) */
#define http_globals_ctor _http_globals_ctor
static inline void _http_globals_ctor(zend_http_globals *http_globals)
{
	http_globals->etag_started = 0;
	http_globals->ctype = NULL;
	http_globals->etag  = NULL;
	http_globals->lmod  = 0;
#ifdef HTTP_HAVE_CURL
#	if LIBCURL_VERSION_NUM < 0x070c00
	memset(&http_globals->curlerr, 0, sizeof(http_globals->curlerr));
#	endif
	zend_llist_init(&http_globals->to_free, sizeof(char *), free_to_free, 0);
#endif
	http_globals->allowed_methods = NULL;
	http_globals->cache_log = NULL;
}
/* }}} */

/* {{{ void http_globals_dtor() */
#define http_globals_dtor() _http_globals_dtor(TSRMLS_C)
static inline void _http_globals_dtor(TSRMLS_D)
{
	HTTP_G(etag_started) = 0;
	HTTP_G(lmod) = 0;

	if (HTTP_G(etag)) {
		efree(HTTP_G(etag));
		HTTP_G(etag) = NULL;
	}

	if (HTTP_G(ctype)) {
		efree(HTTP_G(ctype));
		HTTP_G(ctype) = NULL;
	}
	
#ifdef HTTP_HAVE_CURL
#	if LIBCURL_VERSION_NUM < 0x070c00
	memset(&HTTP_G(curlerr), 0, sizeof(HTTP_G(curlerr)));
#	endif
#endif

}
/* }}} */

/* {{{ static inline void http_check_allowed_methods(char *, int) */
#define http_check_allowed_methods(m, l) _http_check_allowed_methods((m), (l) TSRMLS_CC)
static inline void _http_check_allowed_methods(char *methods, int length TSRMLS_DC)
{
	char *found, *header;

	if (!length || !SG(request_info).request_method) {
		return;
	}

	if (	(found = strstr(methods, SG(request_info).request_method)) &&
			(found == SG(request_info).request_method || !isalpha(found[-1])) &&
			(!isalpha(found[strlen(SG(request_info).request_method) + 1]))) {
		return;
	}

	header = emalloc(length + sizeof("Allow: "));
	sprintf(header, "Allow: %s", methods);
	http_exit(405, header);
}
/* }}} */

/* {{{ PHP_INI */
PHP_INI_MH(http_update_allowed_methods)
{
	http_check_allowed_methods(new_value, new_value_length);
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

#define http_update_cache_log OnUpdateString

PHP_INI_BEGIN()
	HTTP_INI_ENTRY("http.allowed_methods", HTTP_KNOWN_METHODS, PHP_INI_ALL, allowed_methods)
	HTTP_INI_ENTRY("http.cache_log", NULL, PHP_INI_ALL, cache_log)
PHP_INI_END()
/* }}} */

/* {{{ HTTP_CURL_USE_ZEND_MM */
#if defined(HTTP_HAVE_CURL) && defined(HTTP_CURL_USE_ZEND_MM)
static void http_curl_free(void *p)					{ efree(p); }
static char *http_curl_strdup(const char *p)		{ return estrdup(p); }
static void *http_curl_malloc(size_t s)				{ return emalloc(s); }
static void *http_curl_realloc(void *p, size_t s)	{ return erealloc(p, s); }
static void *http_curl_calloc(size_t n, size_t s)	{ return ecalloc(n, s); }
#endif /* HTTP_HAVE_CURL && HTTP_CURL_USE_ZEND_MM */
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(http)
{
	ZEND_INIT_MODULE_GLOBALS(http, http_globals_ctor, NULL);
	REGISTER_INI_ENTRIES();

#ifdef HTTP_HAVE_CURL
#	ifdef HTTP_CURL_USE_ZEND_MM
	if (CURLE_OK != curl_global_init_mem(CURL_GLOBAL_ALL,
			http_curl_malloc,
			http_curl_free,
			http_curl_realloc,
			http_curl_strdup,
			http_curl_calloc)) {
		return FAILURE;
	}
#	endif /* HTTP_CURL_USE_ZEND_MM */
#endif /* HTTP_HAVE_CURL */

#ifdef ZEND_ENGINE_2
	http_util_object_init(INIT_FUNC_ARGS_PASSTHRU);
	http_message_object_init(INIT_FUNC_ARGS_PASSTHRU);
	http_response_object_init(INIT_FUNC_ARGS_PASSTHRU);
#	ifdef HTTP_HAVE_CURL
	http_request_object_init(INIT_FUNC_ARGS_PASSTHRU);
#	endif /* HTTP_HAVE_CURL */
	http_exception_object_init(INIT_FUNC_ARGS_PASSTHRU);
#endif /* ZEND_ENGINE_2 */

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(http)
{
	UNREGISTER_INI_ENTRIES();
#ifdef HTTP_HAVE_CURL
	curl_global_cleanup();
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(http)
{
	char *allowed_methods = INI_STR("http.allowed_methods");
	http_check_allowed_methods(allowed_methods, strlen(allowed_methods));
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(http)
{
	http_globals_dtor();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(http)
{
#ifdef ZEND_ENGINE_2
#	define HTTP_FUNC_AVAIL(CLASS) "procedural, object oriented (" CLASS ")"
#else
#	define HTTP_FUNC_AVAIL(CLASS) "procedural"
#endif

#ifdef HTTP_HAVE_CURL
#	define HTTP_CURL_VERSION curl_version()
#	ifdef ZEND_ENGINE_2
#		define HTTP_CURL_AVAIL(CLASS) "procedural, object oriented (" CLASS ")"
#	else
#		define HTTP_CURL_AVAIL(CLASS) "procedural"
#	endif
#else
#	define HTTP_CURL_VERSION "libcurl not available"
#	define HTTP_CURL_AVAIL(CLASS) "libcurl not available"
#endif

	char full_version_string[1024] = {0};
	snprintf(full_version_string, 1023, "%s (%s)", HTTP_PEXT_VERSION, HTTP_CURL_VERSION);

	php_info_print_table_start();
	php_info_print_table_row(2, "Extended HTTP support", "enabled");
	php_info_print_table_row(2, "Extension Version:", full_version_string);
	php_info_print_table_end();

	php_info_print_table_start();
	php_info_print_table_header(2, "Functionality",            "Availability");
	php_info_print_table_row(2,    "Miscellaneous Utilities:", HTTP_FUNC_AVAIL("HttpUtil, HttpMessage"));
	php_info_print_table_row(2,    "Extended HTTP Responses:", HTTP_FUNC_AVAIL("HttpResponse"));
	php_info_print_table_row(2,    "Extended HTTP Requests:",  HTTP_CURL_AVAIL("HttpRequest"));
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

