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

#include "php_http_std_defs.h"
#include "php_http_request_object.h"
#include "php_http_request_api.h"

#ifdef ZEND_ENGINE_2
#ifdef HTTP_HAVE_CURL

#define http_request_object_declare_default_properties() _http_request_object_declare_default_properties(TSRMLS_C)
static inline void _http_request_object_declare_default_properties(TSRMLS_D);

zend_class_entry *http_request_object_ce;
zend_function_entry http_request_object_fe[] = {
	PHP_ME(HttpRequest, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpRequest, __destruct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	PHP_ME(HttpRequest, setOptions, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getOptions, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, unsetOptions, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, setSslOptions, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getSslOptions, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, unsetSslOptions, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpRequest, addHeaders, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getHeaders, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, unsetHeaders, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, addCookies, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getCookies, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, unsetCookies, NULL, ZEND_ACC_PUBLIC)

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
	PHP_ME(HttpRequest, getResponseCookie, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getResponseCode, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getResponseBody, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getResponseInfo, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpRequest, getResponseMessage, NULL, ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL}
};
static zend_object_handlers http_request_object_handlers;

void _http_request_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS_EX(HttpRequest, http_request_object, NULL, 0);

	/* HTTP/1.1 */
	HTTP_LONG_CONSTANT("HTTP_GET", HTTP_GET);
	HTTP_LONG_CONSTANT("HTTP_HEAD", HTTP_HEAD);
	HTTP_LONG_CONSTANT("HTTP_POST", HTTP_POST);
	HTTP_LONG_CONSTANT("HTTP_PUT", HTTP_PUT);
	HTTP_LONG_CONSTANT("HTTP_DELETE", HTTP_DELETE);
	HTTP_LONG_CONSTANT("HTTP_OPTIONS", HTTP_OPTIONS);
	HTTP_LONG_CONSTANT("HTTP_TRACE", HTTP_TRACE);
	HTTP_LONG_CONSTANT("HTTP_CONNECT", HTTP_CONNECT);
	/* WebDAV - RFC 2518 */
	HTTP_LONG_CONSTANT("HTTP_PROPFIND", HTTP_PROPFIND);
	HTTP_LONG_CONSTANT("HTTP_PROPPATCH", HTTP_PROPPATCH);
	HTTP_LONG_CONSTANT("HTTP_MKCOL", HTTP_MKCOL);
	HTTP_LONG_CONSTANT("HTTP_COPY", HTTP_COPY);
	HTTP_LONG_CONSTANT("HTTP_MOVE", HTTP_MOVE);
	HTTP_LONG_CONSTANT("HTTP_LOCK", HTTP_LOCK);
	HTTP_LONG_CONSTANT("HTTP_UNLOCK", HTTP_UNLOCK);
	/* WebDAV Versioning - RFC 3253 */
	HTTP_LONG_CONSTANT("HTTP_VERSION_CONTROL", HTTP_VERSION_CONTROL);
	HTTP_LONG_CONSTANT("HTTP_REPORT", HTTP_REPORT);
	HTTP_LONG_CONSTANT("HTTP_CHECKOUT", HTTP_CHECKOUT);
	HTTP_LONG_CONSTANT("HTTP_CHECKIN", HTTP_CHECKIN);
	HTTP_LONG_CONSTANT("HTTP_UNCHECKOUT", HTTP_UNCHECKOUT);
	HTTP_LONG_CONSTANT("HTTP_MKWORKSPACE", HTTP_MKWORKSPACE);
	HTTP_LONG_CONSTANT("HTTP_UPDATE", HTTP_UPDATE);
	HTTP_LONG_CONSTANT("HTTP_LABEL", HTTP_LABEL);
	HTTP_LONG_CONSTANT("HTTP_MERGE", HTTP_MERGE);
	HTTP_LONG_CONSTANT("HTTP_BASELINE_CONTROL", HTTP_BASELINE_CONTROL);
	HTTP_LONG_CONSTANT("HTTP_MKACTIVITY", HTTP_MKACTIVITY);
	/* WebDAV Access Control - RFC 3744 */
	HTTP_LONG_CONSTANT("HTTP_ACL", HTTP_ACL);


#	if LIBCURL_VERSION_NUM >= 0x070a05
	HTTP_LONG_CONSTANT("HTTP_AUTH_BASIC", CURLAUTH_BASIC);
	HTTP_LONG_CONSTANT("HTTP_AUTH_DIGEST", CURLAUTH_DIGEST);
	HTTP_LONG_CONSTANT("HTTP_AUTH_NTLM", CURLAUTH_NTLM);
#	endif /* LIBCURL_VERSION_NUM */
}

zend_object_value _http_request_object_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	http_request_object *o;

	o = ecalloc(1, sizeof(http_request_object));
	o->zo.ce = ce;
	o->ch = curl_easy_init();

	phpstr_init_ex(&o->response, HTTP_CURLBUF_SIZE, 0);

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = zend_objects_store_put(o, (zend_objects_store_dtor_t) zend_objects_destroy_object, http_request_object_free, NULL TSRMLS_CC);
	ov.handlers = &http_request_object_handlers;

	return ov;
}

static inline void _http_request_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_request_object_ce;

	DCL_PROP_N(PROTECTED, options);
	DCL_PROP_N(PROTECTED, responseInfo);
	DCL_PROP_N(PROTECTED, responseData);
	DCL_PROP_N(PROTECTED, responseCode);
	DCL_PROP_N(PROTECTED, responseMessage);
	DCL_PROP_N(PROTECTED, postData);
	DCL_PROP_N(PROTECTED, postFiles);

	DCL_PROP(PROTECTED, long, method, HTTP_GET);

	DCL_PROP(PROTECTED, string, url, "");
	DCL_PROP(PROTECTED, string, contentType, "");
	DCL_PROP(PROTECTED, string, queryData, "");
	DCL_PROP(PROTECTED, string, postData, "");
}

void _http_request_object_free(zend_object *object TSRMLS_DC)
{
	http_request_object *o = (http_request_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	if (o->ch) {
		curl_easy_cleanup(o->ch);
	}
	phpstr_dtor(&o->response);
	efree(o);
}

#endif /* HTTP_HAVE_CURL */
#endif /* ZEND_ENGINE_2 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

