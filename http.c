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

#define _WINSOCKAPI_
#define ZEND_INCLUDE_FULL_WINDOWS_HEADERS

#ifdef HAVE_CONFIG_H
#	include "config.h"
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

#ifdef ZEND_ENGINE_2
#	include "ext/standard/php_http.h"
#endif

#ifdef HTTP_HAVE_CURL

#	ifdef PHP_WIN32
#		include <winsock2.h>
#		include <sys/types.h>
#	endif

#	include <curl/curl.h>

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

/* {{{ HTTPi */

zend_class_entry *httpi_ce;

#define HTTPi_ME(me, al, ai) ZEND_FENTRY(me, ZEND_FN(al), ai, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)

zend_function_entry httpi_class_methods[] = {
	HTTPi_ME(date, http_date, NULL)
	HTTPi_ME(absoluteURI, http_absolute_uri, NULL)
	HTTPi_ME(negotiateLanguage, http_negotiate_language, NULL)
	HTTPi_ME(negotiateCharset, http_negotiate_charset, NULL)
	HTTPi_ME(redirect, http_redirect, NULL)
	HTTPi_ME(sendStatus, http_send_status, NULL)
	HTTPi_ME(sendLastModified, http_send_last_modified, NULL)
	HTTPi_ME(sendContentType, http_send_content_type, NULL)
	HTTPi_ME(sendContentDisposition, http_send_content_disposition, NULL)
	HTTPi_ME(matchModified, http_match_modified, NULL)
	HTTPi_ME(matchEtag, http_match_etag, NULL)
	HTTPi_ME(cacheLastModified, http_cache_last_modified, NULL)
	HTTPi_ME(cacheEtag, http_cache_etag, NULL)
	HTTPi_ME(chunkedDecode, http_chunked_decode, NULL)
	HTTPi_ME(splitResponse, http_split_response, NULL)
	HTTPi_ME(parseHeaders, http_parse_headers, NULL)
	HTTPi_ME(getRequestHeaders, http_get_request_headers, NULL)
#ifdef HTTP_HAVE_CURL
	HTTPi_ME(get, http_get, http_request_info_ref_3)
	HTTPi_ME(head, http_head, http_request_info_ref_3)
	HTTPi_ME(postData, http_post_data, http_request_info_ref_4)
	HTTPi_ME(postArray, http_post_array, http_request_info_ref_4)
#endif
	HTTPi_ME(authBasic, http_auth_basic, NULL)
	HTTPi_ME(authBasicCallback, http_auth_basic_cb, NULL)
	{NULL, NULL, NULL}
};
/* }}} HTTPi */

/* {{{ HTTPi_Response */

zend_class_entry *httpi_response_ce;
static zend_object_handlers httpi_response_object_handlers;

#define httpi_response_declare_default_properties(ce) _httpi_response_declare_default_properties(ce TSRMLS_CC)
static inline void _httpi_response_declare_default_properties(zend_class_entry *ce TSRMLS_DC)
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

#define httpi_response_destroy_object _httpi_response_destroy_object
void _httpi_response_destroy_object(void *object, zend_object_handle handle TSRMLS_DC)
{
	httpi_response_object *o = object;
	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	efree(o);
}

#define httpi_response_new_object _httpi_response_new_object
zend_object_value _httpi_response_new_object(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	httpi_response_object *o;

	o = ecalloc(1, sizeof(httpi_response_object));
	o->zo.ce = ce;

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = zend_objects_store_put(o, httpi_response_destroy_object, NULL, NULL TSRMLS_CC);
	ov.handlers = &httpi_response_object_handlers;

	return ov;
}

zend_function_entry httpi_response_class_methods[] = {
	PHP_ME(HTTPi_Response, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
/*	PHP_ME(HTTPi_Response, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)
*/
	PHP_ME(HTTPi_Response, setETag, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getETag, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setContentDisposition, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getContentDisposition, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setContentType, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getContentType, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setCache, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getCache, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setCacheControl, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getCacheControl, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setGzip, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getGzip, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getData, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setFile, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getFile, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, setStream, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Response, getStream, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Response, send, NULL, ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ HTTPi_Request */
#ifdef HTTP_HAVE_CURL

zend_class_entry *httpi_request_ce;
static zend_object_handlers httpi_request_object_handlers;

#define httpi_request_declare_default_properties(ce) _httpi_request_declare_default_properties(ce TSRMLS_CC)
static inline void _httpi_request_declare_default_properties(zend_class_entry *ce TSRMLS_DC)
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

#define httpi_request_free_object _httpi_request_free_object
void _httpi_request_free_object(zend_object /* void */ *object TSRMLS_DC)
{
	httpi_request_object *o = (httpi_request_object *) object;

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

#define httpi_request_new_object _httpi_request_new_object
zend_object_value _httpi_request_new_object(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	httpi_request_object *o;

	o = ecalloc(1, sizeof(httpi_request_object));
	o->zo.ce = ce;
	o->ch = curl_easy_init();

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = zend_objects_store_put(o, (zend_objects_store_dtor_t) zend_objects_destroy_object, httpi_request_free_object, NULL TSRMLS_CC);
	ov.handlers = &httpi_request_object_handlers;

	return ov;
}

zend_function_entry httpi_request_class_methods[] = {
	PHP_ME(HTTPi_Request, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HTTPi_Request, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	PHP_ME(HTTPi_Request, setOptions, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getOptions, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Request, setMethod, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getMethod, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Request, setURL, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getURL, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Request, setContentType, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getContentType, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Request, setQueryData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getQueryData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, addQueryData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, unsetQueryData, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Request, setPostData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getPostData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, addPostData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, unsetPostData, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Request, addPostFile, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getPostFiles, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, unsetPostFiles, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Request, send, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HTTPi_Request, getResponseData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getResponseHeaders, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getResponseBody, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HTTPi_Request, getResponseInfo, NULL, ZEND_ACC_PUBLIC)

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
	PHP_EXT_HTTP_VERSION,
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
	http_globals->curlbuf.data = NULL;
	http_globals->curlbuf.used = 0;
	http_globals->curlbuf.free = 0;
	http_globals->curlbuf.size = 0;
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
	STD_PHP_INI_ENTRY("http.allowed_methods", "OPTIONS,GET,HEAD,POST,PUT,DELETE,TRACE,CONNECT", PHP_INI_ALL, update_allowed_methods, allowed_methods, zend_http_globals, http_globals)
PHP_INI_END()
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(http)
{
	ZEND_INIT_MODULE_GLOBALS(http, php_http_init_globals, NULL);
	REGISTER_INI_ENTRIES();

#if defined(HTTP_HAVE_CURL) && (LIBCURL_VERSION_NUM >= 0x070a05)
	REGISTER_LONG_CONSTANT("HTTP_AUTH_BASIC", CURLAUTH_BASIC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_AUTH_DIGEST", CURLAUTH_DIGEST, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_AUTH_NTLM", CURLAUTH_NTLM, CONST_CS | CONST_PERSISTENT);
#endif

#ifdef ZEND_ENGINE_2
	HTTP_REGISTER_CLASS(HTTPi, httpi, NULL, ZEND_ACC_FINAL_CLASS);
	HTTP_REGISTER_CLASS_EX(HTTPi_Response, httpi_response, NULL, 0);
#	ifdef HTTP_HAVE_CURL
	HTTP_REGISTER_CLASS_EX(HTTPi_Request, httpi_request, NULL, 0);
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
	if (HTTP_G(curlbuf).data) {
		efree(HTTP_G(curlbuf).data);
		HTTP_G(curlbuf).data = NULL;
		HTTP_G(curlbuf).used = 0;
		HTTP_G(curlbuf).free = 0;
	}
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(http)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Extended HTTP support", "enabled");
	php_info_print_table_row(2, "Version:", PHP_EXT_HTTP_VERSION);
	php_info_print_table_row(2, "cURL convenience functions:",
#ifdef HTTP_HAVE_CURL
			"enabled"
#else
			"disabled"
#endif
	);
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

