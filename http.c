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
#include "php_ini.h"
#include "ext/standard/info.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_send_api.h"
#ifdef HTTP_HAVE_CURL
#	include "php_http_request_api.h"
#endif

#ifdef ZEND_ENGINE_2
#	include "php_http_util_object.h"
#	include "php_http_message_object.h"
#	include "php_http_response_object.h"
#	ifdef HTTP_HAVE_CURL
#		include "php_http_request_object.h"
#		include "php_http_requestpool_object.h"
#	endif
#	include "php_http_exception_object.h"
#endif

#include "missing.h"
#include "phpstr/phpstr.h"

#ifdef HTTP_HAVE_CURL
#	ifdef PHP_WIN32
#		include <winsock2.h>
#	endif
#	include <curl/curl.h>
#endif

#include <ctype.h>

ZEND_DECLARE_MODULE_GLOBALS(http);
HTTP_DECLARE_ARG_PASS_INFO();

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
	PHP_FE(http_throttle, NULL)
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
	PHP_FE(http_parse_message, NULL)
	PHP_FE(http_parse_headers, NULL)
	PHP_FE(http_get_request_headers, NULL)
	PHP_FE(http_get_request_body, NULL)
	PHP_FE(http_match_request_header, NULL)
#ifdef HTTP_HAVE_CURL
	PHP_FE(http_get, http_arg_pass_ref_3)
	PHP_FE(http_head, http_arg_pass_ref_3)
	PHP_FE(http_post_data, http_arg_pass_ref_4)
	PHP_FE(http_post_fields, http_arg_pass_ref_5)
	PHP_FE(http_put_file, http_arg_pass_ref_4)
	PHP_FE(http_put_stream, http_arg_pass_ref_4)
	PHP_FE(http_request_method_register, NULL)
	PHP_FE(http_request_method_unregister, NULL)
	PHP_FE(http_request_method_exists, NULL)
	PHP_FE(http_request_method_name, NULL)
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

int http_module_number;

/* {{{ http_globals */
static inline void http_globals_init(zend_http_globals *G)
{
	memset(G, 0, sizeof(zend_http_globals));
	G->send.buffer_size = HTTP_SENDBUF_SIZE;
	zend_hash_init(&G->request.methods.custom, 0, NULL, ZVAL_PTR_DTOR, 0);
#ifdef HTTP_HAVE_CURL
	zend_llist_init(&G->request.copies.strings, sizeof(char *), http_request_data_free_string, 0);
	zend_llist_init(&G->request.copies.slists, sizeof(struct curl_slist *), http_request_data_free_slist, 0);
	zend_llist_init(&G->request.copies.contexts, sizeof(http_curl_callback_ctx *), http_request_data_free_context, 0);
	zend_llist_init(&G->request.copies.convs, sizeof(http_curl_conv *), http_request_data_free_conv, 0);
#endif
}

static inline void http_globals_free(zend_http_globals *G)
{
	STR_FREE(G->send.content_type);
	STR_FREE(G->send.unquoted_etag);
	zend_hash_destroy(&G->request.methods.custom);
#ifdef HTTP_HAVE_CURL
	zend_llist_clean(&G->request.copies.strings);
	zend_llist_clean(&G->request.copies.slists);
	zend_llist_clean(&G->request.copies.contexts);
	zend_llist_clean(&G->request.copies.convs);
#endif
}
/* }}} */

/* {{{ static inline void http_check_allowed_methods(char *, int) */
#define http_check_allowed_methods(m, l) _http_check_allowed_methods((m), (l) TSRMLS_CC)
static inline void _http_check_allowed_methods(char *methods, int length TSRMLS_DC)
{
	if (length && SG(request_info).request_method) {
		if (SUCCESS != http_check_method_ex(SG(request_info).request_method, methods)) {
			char *header = emalloc(length + sizeof("Allow: "));
			sprintf(header, "Allow: %s", methods);
			http_exit(405, header);
		}
	}
}
/* }}} */

/* {{{ PHP_INI */
PHP_INI_MH(http_update_allowed_methods)
{
	http_check_allowed_methods(new_value, new_value_length);
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

PHP_INI_BEGIN()
	HTTP_PHP_INI_ENTRY("http.allowed_methods", "", PHP_INI_ALL, http_update_allowed_methods, request.methods.allowed)
	HTTP_PHP_INI_ENTRY("http.cache_log", "", PHP_INI_ALL, OnUpdateString, log.cache)
	HTTP_PHP_INI_ENTRY("http.redirect_log", "", PHP_INI_ALL, OnUpdateString, log.redirect)
	HTTP_PHP_INI_ENTRY("http.allowed_methods_log", "", PHP_INI_ALL, OnUpdateString, log.allowed_methods)
	HTTP_PHP_INI_ENTRY("http.composite_log", "", PHP_INI_ALL, OnUpdateString, log.composite)
#ifdef ZEND_ENGINE_2
	HTTP_PHP_INI_ENTRY("http.only_exceptions", "0", PHP_INI_ALL, OnUpdateBool, only_exceptions)
#endif
PHP_INI_END()
/* }}} */


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(http)
{
	http_module_number = module_number;

#ifdef ZTS
	ZEND_INIT_MODULE_GLOBALS(http, NULL, NULL)
#endif

	REGISTER_INI_ENTRIES();

#ifdef HTTP_HAVE_CURL
	if (CURLE_OK != curl_global_init(CURL_GLOBAL_ALL)) {
		return FAILURE;
	}
#endif /* HTTP_HAVE_CURL */

#ifdef ZEND_ENGINE_2
	http_util_object_init();
	http_message_object_init();
	http_response_object_init();
#	ifdef HTTP_HAVE_CURL
	http_request_object_init();
	http_requestpool_object_init();
#	endif /* HTTP_HAVE_CURL */
	http_exception_object_init();
#endif /* ZEND_ENGINE_2 */

	zend_hash_init_ex(&http_response_statics, 0, NULL, ZVAL_INTERNAL_PTR_DTOR, 1, 0);
	zend_fix_static_properties(http_response_object_ce, &http_response_statics TSRMLS_CC);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(http)
{
	zend_hash_destroy(&http_response_statics);
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
	char *m;

	if (m = INI_STR("http.allowed_methods")) {
		http_check_allowed_methods(m, strlen(m));
	}

	http_globals_init(HTTP_GLOBALS);
	zend_init_static_properties(http_response_object_ce, &http_response_statics TSRMLS_CC);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(http)
{
	zend_clean_static_properties(http_response_object_ce TSRMLS_CC);
	http_globals_free(HTTP_GLOBALS);
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

#include "php_http_request_api.h"

	php_info_print_table_start();
	{
		char full_version_string[1024] = {0};
		snprintf(full_version_string, 1023, "%s (%s)", HTTP_PEXT_VERSION, HTTP_CURL_VERSION);

		php_info_print_table_row(2, "Extended HTTP support:", "enabled");
		php_info_print_table_row(2, "Extension Version:", full_version_string);
	}
	php_info_print_table_end();

#ifdef HTTP_HAVE_CURL
	php_info_print_table_start();
	{
		unsigned i;
		zval **custom_method;
		phpstr *known_request_methods = phpstr_new();
		phpstr *custom_request_methods = phpstr_new();

		for (i = HTTP_NO_REQUEST_METHOD+1; i < HTTP_MAX_REQUEST_METHOD; ++i) {
			phpstr_appendl(known_request_methods, http_request_method_name(i));
			phpstr_appends(known_request_methods, ", ");
		}
		FOREACH_HASH_VAL(&HTTP_G(request).methods.custom, custom_method) {
			phpstr_append(custom_request_methods, Z_STRVAL_PP(custom_method), Z_STRLEN_PP(custom_method));
			phpstr_appends(custom_request_methods, ", ");
		}

		phpstr_append(known_request_methods, PHPSTR_VAL(custom_request_methods), PHPSTR_LEN(custom_request_methods));
		phpstr_fix(known_request_methods);
		phpstr_fix(custom_request_methods);

		php_info_print_table_row(2, "Known Request Methods:", PHPSTR_VAL(known_request_methods));
		php_info_print_table_row(2, "Custom Request Methods:",
			PHPSTR_LEN(custom_request_methods) ? PHPSTR_VAL(custom_request_methods) : "none registered");

		phpstr_free(&known_request_methods);
		phpstr_free(&custom_request_methods);
	}
	php_info_print_table_end();
#endif

	php_info_print_table_start();
	{
		php_info_print_table_header(2, "Functionality",            "Availability");
		php_info_print_table_row(2,    "Miscellaneous Utilities:", HTTP_FUNC_AVAIL("HttpUtil, HttpMessage"));
		php_info_print_table_row(2,    "Extended HTTP Responses:", HTTP_FUNC_AVAIL("HttpResponse"));
		php_info_print_table_row(2,    "Extended HTTP Requests:",  HTTP_CURL_AVAIL("HttpRequest, HttpRequestPool"));
	}
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

