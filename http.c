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

#include "php.h"
#include "php_ini.h"
#include "snprintf.h"
#include "ext/standard/info.h"
#include "ext/session/php_session.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_smart_str.h"

#include "SAPI.h"

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_curl_api.h"
#include "php_http_std_defs.h"

#include "phpstr/phpstr.h"

#ifdef HTTP_HAVE_CURL
/* {{{ ARG_INFO */
#	ifdef ZEND_BEGIN_ARG_INFO
ZEND_BEGIN_ARG_INFO(http_request_info_ref_3, 0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO();

ZEND_BEGIN_ARG_INFO(http_request_info_ref_4, 0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(0)
	ZEND_ARG_PASS_INFO(1)
ZEND_END_ARG_INFO();
#	else
static unsigned char http_request_info_ref_3[] = {3, BYREF_NONE, BYREF_NONE, BYREF_FORCE};
static unsigned char http_request_info_ref_4[] = {4, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_FORCE};
#	endif
/* }}} ARG_INFO */

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
	PHP_FE(ob_httpetaghandler, NULL)
	{NULL, NULL, NULL}
};
/* }}} */


#ifdef ZEND_ENGINE_2

/* {{{ HttpUtil */

zend_class_entry *http_util_ce;

#define HTTP_UTIL_ME(me, al, ai) ZEND_FENTRY(me, ZEND_FN(al), ai, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)

zend_function_entry http_util_class_methods[] = {
	HTTP_UTIL_ME(date, http_date, NULL)
	HTTP_UTIL_ME(absoluteURI, http_absolute_uri, NULL)
	HTTP_UTIL_ME(negotiateLanguage, http_negotiate_language, NULL)
	HTTP_UTIL_ME(negotiateCharset, http_negotiate_charset, NULL)
	HTTP_UTIL_ME(redirect, http_redirect, NULL)
	HTTP_UTIL_ME(sendStatus, http_send_status, NULL)
	HTTP_UTIL_ME(sendLastModified, http_send_last_modified, NULL)
	HTTP_UTIL_ME(sendContentType, http_send_content_type, NULL)
	HTTP_UTIL_ME(sendContentDisposition, http_send_content_disposition, NULL)
	HTTP_UTIL_ME(matchModified, http_match_modified, NULL)
	HTTP_UTIL_ME(matchEtag, http_match_etag, NULL)
	HTTP_UTIL_ME(cacheLastModified, http_cache_last_modified, NULL)
	HTTP_UTIL_ME(cacheEtag, http_cache_etag, NULL)
	HTTP_UTIL_ME(chunkedDecode, http_chunked_decode, NULL)
	HTTP_UTIL_ME(splitResponse, http_split_response, NULL)
	HTTP_UTIL_ME(parseHeaders, http_parse_headers, NULL)
	HTTP_UTIL_ME(getRequestHeaders, http_get_request_headers, NULL)
#ifdef HTTP_HAVE_CURL
	HTTP_UTIL_ME(get, http_get, http_request_info_ref_3)
	HTTP_UTIL_ME(head, http_head, http_request_info_ref_3)
	HTTP_UTIL_ME(postData, http_post_data, http_request_info_ref_4)
	HTTP_UTIL_ME(postArray, http_post_array, http_request_info_ref_4)
#endif
	HTTP_UTIL_ME(authBasic, http_auth_basic, NULL)
	HTTP_UTIL_ME(authBasicCallback, http_auth_basic_cb, NULL)
	{NULL, NULL, NULL}
};
/* }}} HttpUtil */

/* {{{ HttpResponse */

zend_class_entry *http_response_ce;
static zend_object_handlers http_response_object_handlers;

#define http_response_declare_default_properties(ce) _http_response_declare_default_properties(ce TSRMLS_CC)
static inline void _http_response_declare_default_properties(zend_class_entry *ce TSRMLS_DC)
{
	DCL_PROP(PROTECTED, string, contentType, "application/x-octetstream");
	DCL_PROP(PROTECTED, string, eTag, "");
	DCL_PROP(PROTECTED, string, dispoFile, "");
	DCL_PROP(PROTECTED, string, cacheControl, "public");
	DCL_PROP(PROTECTED, string, data, "");
	DCL_PROP(PROTECTED, string, file, "");
	DCL_PROP(PROTECTED, long, stream, 0);
	DCL_PROP(PROTECTED, long, lastModified, 0);
	DCL_PROP(PROTECTED, long, dispoInline, 0);
	DCL_PROP(PROTECTED, long, cache, 0);
	DCL_PROP(PROTECTED, long, gzip, 0);

	DCL_PROP(PRIVATE, long, raw_cache_header, 0);
	DCL_PROP(PRIVATE, long, send_mode, -1);
}

#define http_response_destroy_object _http_response_destroy_object
void _http_response_destroy_object(void *object, zend_object_handle handle TSRMLS_DC)
{
	http_response_object *o = object;
	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	efree(o);
}

#define http_response_new_object _http_response_new_object
zend_object_value _http_response_new_object(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	http_response_object *o;

	o = ecalloc(1, sizeof(http_response_object));
	o->zo.ce = ce;

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = zend_objects_store_put(o, http_response_destroy_object, NULL, NULL TSRMLS_CC);
	ov.handlers = &http_response_object_handlers;

	return ov;
}

zend_function_entry http_response_class_methods[] = {
	PHP_ME(HttpResponse, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
/*	PHP_ME(HttpResponse, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
*/
	PHP_ME(HttpResponse, setETag, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getETag, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setContentDisposition, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getContentDisposition, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setContentType, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getContentType, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setCache, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getCache, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setCacheControl, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getCacheControl, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setGzip, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getGzip, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getData, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setFile, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getFile, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setStream, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getStream, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, send, NULL, ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ HttpRequest */
#ifdef HTTP_HAVE_CURL

zend_class_entry *http_request_ce;
static zend_object_handlers http_request_object_handlers;

#define http_request_declare_default_properties(ce) _http_request_declare_default_properties(ce TSRMLS_CC)
static inline void _http_request_declare_default_properties(zend_class_entry *ce TSRMLS_DC)
{
	DCL_PROP_N(PROTECTED, options);
	DCL_PROP_N(PROTECTED, responseInfo);
	DCL_PROP_N(PROTECTED, responseData);
	DCL_PROP_N(PROTECTED, postData);
	DCL_PROP_N(PROTECTED, postFiles);

	DCL_PROP(PROTECTED, long, method, HTTP_GET);

	DCL_PROP(PROTECTED, string, url, "");
	DCL_PROP(PROTECTED, string, contentType, "");
	DCL_PROP(PROTECTED, string, queryData, "");
	DCL_PROP(PROTECTED, string, postData, "");
}

#define http_request_free_object _http_request_free_object
void _http_request_free_object(zend_object /* void */ *object TSRMLS_DC)
{
	http_request_object *o = (http_request_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	if (o->ch) {
		curl_easy_cleanup(o->ch);
		o->ch = NULL;
	}
	efree(o);
}

#define http_request_new_object _http_request_new_object
zend_object_value _http_request_new_object(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	http_request_object *o;

	o = ecalloc(1, sizeof(http_request_object));
	o->zo.ce = ce;
	o->ch = curl_easy_init();

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = zend_objects_store_put(o, (zend_objects_store_dtor_t) zend_objects_destroy_object, http_request_free_object, NULL TSRMLS_CC);
	ov.handlers = &http_request_object_handlers;

	return ov;
}

zend_function_entry http_request_class_methods[] = {
	PHP_ME(HttpRequest, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpRequest, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	PHP_ME(HttpRequest, setOptions, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getOptions, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, unsetOptions, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, addHeader, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, addCookie, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, setMethod, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getMethod, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, setURL, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getURL, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, setContentType, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getContentType, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, setQueryData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getQueryData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, addQueryData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, unsetQueryData, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, setPostData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getPostData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, addPostData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, unsetPostData, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, addPostFile, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getPostFiles, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, unsetPostFiles, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, send, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, getResponseData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getResponseHeader, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getResponseCode, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getResponseBody, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getResponseInfo, NULL, ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL}
};

#endif /* HTTP_HAVE_CURL */
/* }}} */

#endif /* ZEND_ENGINE_2 */

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

/* {{{ php_http_init_globals(zend_http_globals *) */
static void php_http_init_globals(zend_http_globals *http_globals)
{
	http_globals->etag_started = 0;
	http_globals->ctype = NULL;
	http_globals->etag  = NULL;
	http_globals->lmod  = 0;
#ifdef HTTP_HAVE_CURL
	//phpstr_init_ex(&http_globals->curlbuf, HTTP_CURLBUF_SIZE, 1);
	zend_llist_init(&http_globals->to_free, sizeof(char *), free_to_free, 0);
#endif
	http_globals->allowed_methods = NULL;
}
/* }}} */

/* {{{ static inline STATUS http_check_allowed_methods(char *, int) */
#define http_check_allowed_methods(m, l) _http_check_allowed_methods((m), (l) TSRMLS_CC)
static inline void _http_check_allowed_methods(char *methods, int length TSRMLS_DC)
{
	if (length && SG(request_info).request_method && (!strstr(methods, SG(request_info).request_method))) {
		char *allow_header = emalloc(length + sizeof("Allow: "));
		sprintf(allow_header, "Allow: %s", methods);
		http_send_header(allow_header);
		efree(allow_header);
		http_send_status(405);
		zend_bailout();
	}
}
/* }}} */

/* {{{ PHP_INI */
PHP_INI_MH(update_allowed_methods)
{
	http_check_allowed_methods(new_value, new_value_length);
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

PHP_INI_BEGIN()
	STD_PHP_INI_ENTRY("http.allowed_methods",
		/* HTTP 1.1 */
		"GET, HEAD, POST, PUT, DELETE, OPTIONS, TRACE, CONNECT, "
		/* WebDAV - RFC 2518 * /
		"PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK, "
		/* WebDAV Versioning - RFC 3253 * /
		"VERSION-CONTROL, REPORT, CHECKOUT, CHECKIN, UNCHECKOUT, "
		"MKWORKSPACE, UPDATE, LABEL, MERGE, BASELINE-CONTROL, MKACTIVITY, "
		/* WebDAV Access Control - RFC 3744 * /
		"ACL, "
		/* END */
		,
		PHP_INI_ALL, update_allowed_methods, allowed_methods, zend_http_globals, http_globals)
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
	ZEND_INIT_MODULE_GLOBALS(http, php_http_init_globals, NULL);
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

#	if LIBCURL_VERSION_NUM >= 0x070a05
	REGISTER_LONG_CONSTANT("HTTP_AUTH_BASIC", CURLAUTH_BASIC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_AUTH_DIGEST", CURLAUTH_DIGEST, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_AUTH_NTLM", CURLAUTH_NTLM, CONST_CS | CONST_PERSISTENT);
#	endif /* LIBCURL_VERSION_NUM */
#endif /* HTTP_HAVE_CURL */

#ifdef ZEND_ENGINE_2
	HTTP_REGISTER_CLASS(HttpUtil, http_util, NULL, ZEND_ACC_FINAL_CLASS);
	HTTP_REGISTER_CLASS_EX(HttpResponse, http_response, NULL, 0);
#	ifdef HTTP_HAVE_CURL
	HTTP_REGISTER_CLASS_EX(HttpRequest, http_request, NULL, 0);

	REGISTER_LONG_CONSTANT("HTTP_GET", HTTP_GET, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_HEAD", HTTP_HEAD, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_POST", HTTP_POST, CONST_CS | CONST_PERSISTENT);
#	endif /* HTTP_HAVE_CURL */
#endif /* ZEND_ENGINE_2 */
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(http)
{
	UNREGISTER_INI_ENTRIES();
#ifdef HTTP_HAVE_CURL
	//phpstr_free(&HTTP_G(curlbuf));
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
	//phpstr_free(&HTTP_G(curlbuf));
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(http)
{
#ifdef ZEND_ENGINE_2
#	define HTTP_FUNC_AVAIL(CLASS) "procedural, object oriented (class " CLASS ")"
#else
#	define HTTP_FUNC_AVAIL(CLASS) "procedural"
#endif

#ifdef HTTP_HAVE_CURL
#	define HTTP_CURL_VERSION curl_version()
#	ifdef ZEND_ENGINE_2
#		define HTTP_CURL_AVAIL(CLASS) "procedural, object oriented (class " CLASS ")"
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
	php_info_print_table_row(2,    "Miscellaneous Utilities:", HTTP_FUNC_AVAIL("HttpUtil"));
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

