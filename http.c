/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_SAPI
#define HTTP_WANT_CURL
#define HTTP_WANT_ZLIB
#define HTTP_WANT_MAGIC
#include "php_http.h"

#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_extensions.h"

#include "php_http_api.h"
#include "php_http_send_api.h"
#include "php_http_cookie_api.h"
#include "php_http_cache_api.h"
#include "php_http_send_api.h"
#include "php_http_message_api.h"
#include "php_http_persistent_handle_api.h"
#include "php_http_request_method_api.h"
#ifdef HTTP_HAVE_CURL
#	include "php_http_request_api.h"
#	include "php_http_request_pool_api.h"
#	include "php_http_request_datashare_api.h"
#endif
#ifdef HTTP_HAVE_ZLIB
#	include "php_http_encoding_api.h"
#endif
#include "php_http_url_api.h"

#ifdef ZEND_ENGINE_2
#	include "php_http_filter_api.h"
#	include "php_http_util_object.h"
#	include "php_http_message_object.h"
#	include "php_http_querystring_object.h"
#	ifndef WONKY
#		include "php_http_response_object.h"
#	endif
#	ifdef HTTP_HAVE_CURL
#		include "php_http_request_object.h"
#		include "php_http_requestpool_object.h"
#		include "php_http_requestdatashare_object.h"
#	endif
#	ifdef HTTP_HAVE_ZLIB
#		include "php_http_deflatestream_object.h"
#		include "php_http_inflatestream_object.h"
#	endif
#	include "php_http_exception_object.h"
#endif


ZEND_DECLARE_MODULE_GLOBALS(http);
HTTP_DECLARE_ARG_PASS_INFO();

#ifdef COMPILE_DL_HTTP
ZEND_GET_MODULE(http)
#endif

/* {{{ http_functions[] */
zend_function_entry http_functions[] = {
	PHP_FE(http_date, NULL)
	PHP_FE(http_build_url, http_arg_pass_ref_4)
	PHP_FE(http_build_str, NULL)
#ifndef ZEND_ENGINE_2
	PHP_FALIAS(http_build_query, http_build_str, NULL)
#endif
	PHP_FE(http_negotiate_language, http_arg_pass_ref_2)
	PHP_FE(http_negotiate_charset, http_arg_pass_ref_2)
	PHP_FE(http_negotiate_content_type, http_arg_pass_ref_2)
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
	PHP_FE(http_parse_cookie, NULL)
	PHP_FE(http_build_cookie, NULL)
	PHP_FE(http_parse_params, NULL)
	PHP_FE(http_get_request_headers, NULL)
	PHP_FE(http_get_request_body, NULL)
	PHP_FE(http_get_request_body_stream, NULL)
	PHP_FE(http_match_request_header, NULL)
	PHP_FE(http_persistent_handles_count, NULL)
	PHP_FE(http_persistent_handles_clean, NULL)
	PHP_FE(http_persistent_handles_ident, NULL)
#ifdef HTTP_HAVE_CURL
	PHP_FE(http_get, http_arg_pass_ref_3)
	PHP_FE(http_head, http_arg_pass_ref_3)
	PHP_FE(http_post_data, http_arg_pass_ref_4)
	PHP_FE(http_post_fields, http_arg_pass_ref_5)
	PHP_FE(http_put_data, http_arg_pass_ref_4)
	PHP_FE(http_put_file, http_arg_pass_ref_4)
	PHP_FE(http_put_stream, http_arg_pass_ref_4)
	PHP_FE(http_request, http_arg_pass_ref_5)
	PHP_FE(http_request_body_encode, NULL)
#endif
	PHP_FE(http_request_method_register, NULL)
	PHP_FE(http_request_method_unregister, NULL)
	PHP_FE(http_request_method_exists, NULL)
	PHP_FE(http_request_method_name, NULL)
	PHP_FE(ob_etaghandler, NULL)
#ifdef HTTP_HAVE_ZLIB
	PHP_FE(http_deflate, NULL)
	PHP_FE(http_inflate, NULL)
	PHP_FE(ob_deflatehandler, NULL)
	PHP_FE(ob_inflatehandler, NULL)
#endif
	PHP_FE(http_support, NULL)
	
	EMPTY_FUNCTION_ENTRY
};
/* }}} */

PHP_MINIT_FUNCTION(http);
PHP_MSHUTDOWN_FUNCTION(http);
PHP_RINIT_FUNCTION(http);
PHP_RSHUTDOWN_FUNCTION(http);
PHP_MINFO_FUNCTION(http);

/* {{{ http_module_dep */
#if ZEND_EXTENSION_API_NO >= 220050617
static zend_module_dep http_module_deps[] = {
#	ifdef HTTP_HAVE_SPL
	ZEND_MOD_REQUIRED("spl")
#	endif
#	ifdef HTTP_HAVE_HASH
	ZEND_MOD_REQUIRED("hash")
#	endif
#	ifdef HTTP_HAVE_SESSION
	ZEND_MOD_REQUIRED("session")
#	endif
#	ifdef HTTP_HAVE_ICONV
	ZEND_MOD_REQUIRED("iconv")
#	endif
	{NULL, NULL, NULL, 0}
};
#endif
/* }}} */

/* {{{ http_module_entry */
zend_module_entry http_module_entry = {
#if ZEND_EXTENSION_API_NO >= 220050617
	STANDARD_MODULE_HEADER_EX, NULL,
	http_module_deps,
#else
	STANDARD_MODULE_HEADER,
#endif
	"http",
	http_functions,
	PHP_MINIT(http),
	PHP_MSHUTDOWN(http),
	PHP_RINIT(http),
	PHP_RSHUTDOWN(http),
	PHP_MINFO(http),
	PHP_EXT_HTTP_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

int http_module_number;

/* {{{ http_globals */
static void http_globals_init_once(zend_http_globals *G)
{
	memset(G, 0, sizeof(zend_http_globals));
}

#define http_globals_init(g) _http_globals_init((g) TSRMLS_CC)
static inline void _http_globals_init(zend_http_globals *G TSRMLS_DC)
{
#ifdef HTTP_HAVE_SAPI_RTIME
	G->request.time = sapi_get_request_time(TSRMLS_C);
#else
	G->request.time = time(NULL);
#endif
	G->send.buffer_size = 0;
	G->read_post_data = 0;
}

#define http_globals_free(g) _http_globals_free((g) TSRMLS_CC)
static inline void _http_globals_free(zend_http_globals *G TSRMLS_DC)
{
	if (G->request.headers) {
		zend_hash_destroy(G->request.headers);
		FREE_HASHTABLE(G->request.headers);
		G->request.headers = NULL;
	}
	STR_SET(G->send.content_type, NULL);
	STR_SET(G->send.unquoted_etag, NULL);
	if (G->server_var) {
		zval_ptr_dtor(&G->server_var);
		G->server_var = NULL;
	}
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
PHP_INI_MH(http_update_persistent_handle_ident)
{
	HTTP_G->persistent.handles.ident.h = zend_hash_func(new_value, HTTP_G->persistent.handles.ident.l = new_value_length+1);
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

#ifndef ZEND_ENGINE_2
#	define OnUpdateLong OnUpdateInt
#endif

PHP_INI_BEGIN()
	HTTP_PHP_INI_ENTRY("http.etag.mode", "MD5", PHP_INI_ALL, OnUpdateString, etag.mode)
	HTTP_PHP_INI_ENTRY("http.log.cache", "", PHP_INI_ALL, OnUpdateString, log.cache)
	HTTP_PHP_INI_ENTRY("http.log.redirect", "", PHP_INI_ALL, OnUpdateString, log.redirect)
	HTTP_PHP_INI_ENTRY("http.log.not_found", "", PHP_INI_ALL, OnUpdateString, log.not_found)
	HTTP_PHP_INI_ENTRY("http.log.allowed_methods", "", PHP_INI_ALL, OnUpdateString, log.allowed_methods)
	HTTP_PHP_INI_ENTRY("http.log.composite", "", PHP_INI_ALL, OnUpdateString, log.composite)
	HTTP_PHP_INI_ENTRY("http.request.methods.allowed", "", PHP_INI_ALL, http_update_allowed_methods, request.methods.allowed)
	HTTP_PHP_INI_ENTRY("http.request.methods.custom", "", PHP_INI_PERDIR|PHP_INI_SYSTEM, OnUpdateString, request.methods.custom.ini)
#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)
	HTTP_PHP_INI_ENTRY("http.request.datashare.cookie", "0", PHP_INI_SYSTEM, OnUpdateBool, request.datashare.cookie)
	HTTP_PHP_INI_ENTRY("http.request.datashare.dns", "1", PHP_INI_SYSTEM, OnUpdateBool, request.datashare.dns)
	HTTP_PHP_INI_ENTRY("http.request.datashare.ssl", "0", PHP_INI_SYSTEM, OnUpdateBool, request.datashare.ssl)
	HTTP_PHP_INI_ENTRY("http.request.datashare.connect", "0", PHP_INI_SYSTEM, OnUpdateBool, request.datashare.connect)
#endif
#ifdef HTTP_HAVE_ZLIB
	HTTP_PHP_INI_ENTRY("http.send.inflate.start_auto", "0", PHP_INI_PERDIR|PHP_INI_SYSTEM, OnUpdateBool, send.inflate.start_auto)
	HTTP_PHP_INI_ENTRY("http.send.inflate.start_flags", "0", PHP_INI_ALL, OnUpdateLong, send.inflate.start_flags)
	HTTP_PHP_INI_ENTRY("http.send.deflate.start_auto", "0", PHP_INI_PERDIR|PHP_INI_SYSTEM, OnUpdateBool, send.deflate.start_auto)
	HTTP_PHP_INI_ENTRY("http.send.deflate.start_flags", "0", PHP_INI_ALL, OnUpdateLong, send.deflate.start_flags)
#endif
	HTTP_PHP_INI_ENTRY("http.persistent.handles.limit", "-1", PHP_INI_SYSTEM, OnUpdateLong, persistent.handles.limit)
	HTTP_PHP_INI_ENTRY("http.persistent.handles.ident", "GLOBAL", PHP_INI_ALL, http_update_persistent_handle_ident, persistent.handles.ident.s)
	HTTP_PHP_INI_ENTRY("http.send.not_found_404", "1", PHP_INI_ALL, OnUpdateBool, send.not_found_404)
#ifdef ZEND_ENGINE_2
	HTTP_PHP_INI_ENTRY("http.only_exceptions", "0", PHP_INI_ALL, OnUpdateBool, only_exceptions)
#endif
	HTTP_PHP_INI_ENTRY("http.force_exit", "1", PHP_INI_ALL, OnUpdateBool, force_exit)
PHP_INI_END()
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(http)
{
	http_module_number = module_number;

	ZEND_INIT_MODULE_GLOBALS(http, http_globals_init_once, NULL)

	REGISTER_INI_ENTRIES();
	
	if (	(SUCCESS != PHP_MINIT_CALL(http_persistent_handle)) || /* first */
			(SUCCESS != PHP_MINIT_CALL(http_support))	||
			(SUCCESS != PHP_MINIT_CALL(http_cookie))	||
			(SUCCESS != PHP_MINIT_CALL(http_send))		||
			(SUCCESS != PHP_MINIT_CALL(http_url))		||
			
#ifdef HTTP_HAVE_CURL
			(SUCCESS != PHP_MINIT_CALL(http_request))	||
#	ifdef ZEND_ENGINE_2
			(SUCCESS != PHP_MINIT_CALL(http_request_pool)) ||
			(SUCCESS != PHP_MINIT_CALL(http_request_datashare)) ||
#	endif
#endif /* HTTP_HAVE_CURL */
#ifdef HTTP_HAVE_ZLIB
			(SUCCESS != PHP_MINIT_CALL(http_encoding))	||
#endif
			(SUCCESS != PHP_MINIT_CALL(http_request_method))) {
		return FAILURE;
	}

#ifdef ZEND_ENGINE_2
	if (	(SUCCESS != PHP_MINIT_CALL(http_filter))			||
			(SUCCESS != PHP_MINIT_CALL(http_util_object))		||
			(SUCCESS != PHP_MINIT_CALL(http_message_object))	||
			(SUCCESS != PHP_MINIT_CALL(http_querystring_object))||
#	ifndef WONKY
			(SUCCESS != PHP_MINIT_CALL(http_response_object))	||
#	endif /* WONKY */
#	ifdef HTTP_HAVE_CURL
			(SUCCESS != PHP_MINIT_CALL(http_request_object))	||
			(SUCCESS != PHP_MINIT_CALL(http_requestpool_object))||
			(SUCCESS != PHP_MINIT_CALL(http_requestdatashare_object))||
#	endif /* HTTP_HAVE_CURL */
#	ifdef HTTP_HAVE_ZLIB
			(SUCCESS != PHP_MINIT_CALL(http_deflatestream_object))	||
			(SUCCESS != PHP_MINIT_CALL(http_inflatestream_object))	||
#	endif /* HTTP_HAVE_ZLIB */
			(SUCCESS != PHP_MINIT_CALL(http_exception_object))) {
		return FAILURE;
	}
#endif /* ZEND_ENGINE_2 */

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(http)
{
	UNREGISTER_INI_ENTRIES();
	if (	
#ifdef HTTP_HAVE_CURL
#	ifdef ZEND_ENGINE_2
			(SUCCESS != PHP_MSHUTDOWN_CALL(http_request_datashare)) ||
#	endif
			(SUCCESS != PHP_MSHUTDOWN_CALL(http_request)) ||
#endif
			(SUCCESS != PHP_MSHUTDOWN_CALL(http_persistent_handle)) /* last */
	) {
		return FAILURE;
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(http)
{
	http_globals_init(HTTP_G);

	if (HTTP_G->request.methods.allowed) {
		http_check_allowed_methods(HTTP_G->request.methods.allowed, 
			strlen(HTTP_G->request.methods.allowed));
	}

	if (	(SUCCESS != PHP_RINIT_CALL(http_request_method))
#ifdef HTTP_HAVE_ZLIB	
		||	(SUCCESS != PHP_RINIT_CALL(http_encoding))
#endif
#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)
		||	(SUCCESS != PHP_RINIT_CALL(http_request_datashare))
#endif
	) {
		return FAILURE;
	}
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(http)
{
	STATUS status = SUCCESS;
	
	if (	(SUCCESS != PHP_RSHUTDOWN_CALL(http_request_method))
#ifdef HTTP_HAVE_ZLIB
		||	(SUCCESS != PHP_RSHUTDOWN_CALL(http_encoding))
#endif
#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)
		||	(SUCCESS != PHP_RSHUTDOWN_CALL(http_request_datashare))
#endif
	   ) {
		status = FAILURE;
	}
	
	http_globals_free(HTTP_G);
	return status;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(http)
{
	php_info_print_table_start();
	{
		php_info_print_table_row(2, "HTTP Support", "enabled");
		php_info_print_table_row(2, "Extension Version", PHP_EXT_HTTP_VERSION);
		php_info_print_table_row(2, "Registered Classes",
#ifndef ZEND_ENGINE_2
			"none"
#else
			"HttpUtil, "
			"HttpMessage, "
#	ifdef HTTP_HAVE_CURL
			"HttpRequest, "
			"HttpRequestPool, "
			"HttpRequestDataShare, "
#	endif
#	ifdef HTTP_HAVE_ZLIB
			"HttpDeflateStream, "
			"HttpInflateStream, "
#	endif
#	ifndef WONKY
			"HttpResponse, "
#	endif
			"HttpQueryString"
#endif
		);
		php_info_print_table_row(2, "Output Handlers", "ob_deflatehandler, ob_inflatehandler, ob_etaghandler");
		php_info_print_table_row(2, "Stream Filters", 
#ifndef ZEND_ENGINE_2
			"none"
#else
			"http.chunked_decode, http.chunked_encode, http.deflate, http.inflate"
#endif
		);
	}
	php_info_print_table_end();
	
	php_info_print_table_start();
	php_info_print_table_header(3, "Used Library", "Compiled", "Linked");
	{
#ifdef HTTP_HAVE_CURL
		curl_version_info_data *cv = curl_version_info(CURLVERSION_NOW);
		php_info_print_table_row(3, "libcurl", LIBCURL_VERSION, cv->version);
#else
		php_info_print_table_row(2, "libcurl", "disabled", "disabled");
#endif
#ifdef HTTP_HAVE_ZLIB
		php_info_print_table_row(3, "libz", ZLIB_VERSION, zlibVersion());
#else
		php_info_print_table_row(3, "libz", "disabled", "disabled");
#endif
#if defined(HTTP_HAVE_MAGIC)
		php_info_print_table_row(3, "libmagic", "unknown", "unknown");
#else
		php_info_print_table_row(3, "libmagic", "disabled", "disabled");
#endif
	}
	php_info_print_table_end();
	
	php_info_print_table_start();
	php_info_print_table_colspan_header(4, "Persistent Handles");
	php_info_print_table_header(4, "Provider", "Ident", "Used", "Free");
	{
		HashTable *ht;
		HashPosition pos1, pos2;
		HashKey provider = initHashKey(0), ident = initHashKey(0);
		zval **val, **sub, **zused, **zfree;
		
		if ((ht = http_persistent_handle_statall()) && zend_hash_num_elements(ht)) {
			FOREACH_HASH_KEYVAL(pos1, ht, provider, val) {
				if (zend_hash_num_elements(Z_ARRVAL_PP(val))) {
					FOREACH_KEYVAL(pos2, *val, ident, sub) {
						if (	SUCCESS == zend_hash_find(Z_ARRVAL_PP(sub), ZEND_STRS("used"), (void *) &zused) &&
								SUCCESS == zend_hash_find(Z_ARRVAL_PP(sub), ZEND_STRS("free"), (void *) &zfree)) {
							convert_to_string(*zused);
							convert_to_string(*zfree);
							php_info_print_table_row(4, provider.str, ident.str, Z_STRVAL_PP(zused), Z_STRVAL_PP(zfree));
						} else {
							php_info_print_table_row(4, provider.str, ident.str, "0", "0");
						}
					}
				} else {
					php_info_print_table_row(4, provider.str, "N/A", "0", "0");
				}
			}
		} else {
			php_info_print_table_row(4, "N/A", "N/A", "0", "0");
		}
		if (ht) {
			zend_hash_destroy(ht);
			FREE_HASHTABLE(ht);
		}
	}
	php_info_print_table_end();
	
	php_info_print_table_start();
	php_info_print_table_colspan_header(2, "Request Methods");
	{
		int i;
		phpstr *custom_request_methods = phpstr_new();
		phpstr *known_request_methods = phpstr_from_string(HTTP_KNOWN_METHODS, lenof(HTTP_KNOWN_METHODS));
		http_request_method_entry **ptr = HTTP_G->request.methods.custom.entries;

		for (i = 0; i < HTTP_G->request.methods.custom.count; ++i) {
			if (ptr[i]) {
				phpstr_appendf(custom_request_methods, "%s, ", ptr[i]->name);
			}
		}

		phpstr_append(known_request_methods, PHPSTR_VAL(custom_request_methods), PHPSTR_LEN(custom_request_methods));
		phpstr_fix(known_request_methods);
		phpstr_fix(custom_request_methods);

		php_info_print_table_row(2, "Known", PHPSTR_VAL(known_request_methods));
		php_info_print_table_row(2, "Custom",
			PHPSTR_LEN(custom_request_methods) ? PHPSTR_VAL(custom_request_methods) : "none registered");
		php_info_print_table_row(2, "Allowed", strlen(HTTP_G->request.methods.allowed) ? HTTP_G->request.methods.allowed : "(ANY)");
		
		phpstr_free(&known_request_methods);
		phpstr_free(&custom_request_methods);
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

