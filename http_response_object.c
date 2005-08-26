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

#ifdef ZEND_ENGINE_2

#include "SAPI.h"
#include "php_ini.h"

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_std_defs.h"
#include "php_http_response_object.h"
#include "php_http_exception_object.h"
#include "php_http_send_api.h"
#include "php_http_cache_api.h"

#include "missing.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

#define GET_STATIC_PROP(n)			*GET_STATIC_PROP_EX(http_response_object_ce, n)
#define UPD_STATIC_PROP(t, n, v)	UPD_STATIC_PROP_EX(http_response_object_ce, t, n, v)
#define SET_STATIC_PROP(n, v)		SET_STATIC_PROP_EX(http_response_object_ce, n, v)

#define HTTP_BEGIN_ARGS(method, req_args) 		HTTP_BEGIN_ARGS_EX(HttpResponse, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)		HTTP_EMPTY_ARGS_EX(HttpResponse, method, ret_ref)
#define HTTP_RESPONSE_ME(method, visibility)	PHP_ME(HttpResponse, method, HTTP_ARGS(HttpResponse, method), visibility|ZEND_ACC_STATIC)
#define HTTP_RESPONSE_ALIAS(method, func)		HTTP_STATIC_ME_ALIAS(method, func, HTTP_ARGS(HttpResponse, method))

HTTP_BEGIN_ARGS(setHeader, 2)
	HTTP_ARG_VAL(name, 0)
	HTTP_ARG_VAL(value, 0)
	HTTP_ARG_VAL(replace, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(getHeader, 0)
	HTTP_ARG_VAL(name, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getETag, 0);
HTTP_BEGIN_ARGS(setETag, 1)
	HTTP_ARG_VAL(etag, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getLastModified, 0);
HTTP_BEGIN_ARGS(setLastModified, 1)
	HTTP_ARG_VAL(timestamp, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getCache, 0);
HTTP_BEGIN_ARGS(setCache, 1)
	HTTP_ARG_VAL(cache, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getGzip, 0);
HTTP_BEGIN_ARGS(setGzip, 1)
	HTTP_ARG_VAL(gzip, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getCacheControl, 0);
HTTP_BEGIN_ARGS(setCacheControl, 1)
	HTTP_ARG_VAL(cache_control, 0)
	HTTP_ARG_VAL(max_age, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getContentType, 0);
HTTP_BEGIN_ARGS(setContentType, 1)
	HTTP_ARG_VAL(content_type, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getContentDisposition, 0);
HTTP_BEGIN_ARGS(setContentDisposition, 1)
	HTTP_ARG_VAL(filename, 0)
	HTTP_ARG_VAL(send_inline, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getThrottleDelay, 0);
HTTP_BEGIN_ARGS(setThrottleDelay, 1)
	HTTP_ARG_VAL(seconds, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getBufferSize, 0);
HTTP_BEGIN_ARGS(setBufferSize, 1)
	HTTP_ARG_VAL(bytes, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getData, 0);
HTTP_BEGIN_ARGS(setData, 1)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getStream, 0);
HTTP_BEGIN_ARGS(setStream, 1)
	HTTP_ARG_VAL(stream, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getFile, 0);
HTTP_BEGIN_ARGS(setFile, 1)
	HTTP_ARG_VAL(filepath, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(send, 0)
	HTTP_ARG_VAL(clean_ob, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(capture, 0);

HTTP_BEGIN_ARGS(redirect, 0)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(params, 0)
	HTTP_ARG_VAL(session, 0)
	HTTP_ARG_VAL(permanent, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(status, 1)
	HTTP_ARG_VAL(code, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getRequestHeaders, 0);
HTTP_EMPTY_ARGS(getRequestBody, 0);

#define http_response_object_declare_default_properties() _http_response_object_declare_default_properties(TSRMLS_C)
static inline void _http_response_object_declare_default_properties(TSRMLS_D);

HashTable http_response_statics;

zend_class_entry *http_response_object_ce;
zend_function_entry http_response_object_fe[] = {

	HTTP_RESPONSE_ME(setHeader, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getHeader, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setETag, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getETag, ZEND_ACC_PUBLIC)
	
	HTTP_RESPONSE_ME(setLastModified, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getLastModified, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setContentDisposition, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getContentDisposition, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setContentType, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getContentType, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setCache, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getCache, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setCacheControl, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getCacheControl, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setGzip, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getGzip, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setThrottleDelay, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getThrottleDelay, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setBufferSize, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getBufferSize, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setData, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getData, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setFile, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getFile, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setStream, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getStream, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(send, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(capture, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ALIAS(redirect, http_redirect)
	HTTP_RESPONSE_ALIAS(status, http_send_status)
	HTTP_RESPONSE_ALIAS(getRequestHeaders, http_get_request_headers)
	HTTP_RESPONSE_ALIAS(getRequestBody, http_get_request_body)

	{NULL, NULL, NULL}
};

void _http_response_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS(HttpResponse, http_response_object, NULL, 0);
	http_response_object_declare_default_properties();
}

static inline void _http_response_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_response_object_ce;

	DCL_STATIC_PROP(PRIVATE, bool, sent, 0);
	DCL_STATIC_PROP(PRIVATE, bool, catch, 0);
	DCL_STATIC_PROP(PRIVATE, long, mode, -1);
	DCL_STATIC_PROP(PROTECTED, bool, cache, 0);
	DCL_STATIC_PROP(PROTECTED, bool, gzip, 0);
	DCL_STATIC_PROP(PROTECTED, long, stream, 0);
	DCL_STATIC_PROP_N(PROTECTED, file);
	DCL_STATIC_PROP_N(PROTECTED, data);
	DCL_STATIC_PROP_N(PROTECTED, eTag);
	DCL_STATIC_PROP(PROTECTED, long, lastModified, 0);
	DCL_STATIC_PROP_N(PROTECTED, cacheControl);
	DCL_STATIC_PROP_N(PROTECTED, contentType);
	DCL_STATIC_PROP_N(PROTECTED, contentDisposition);
	DCL_STATIC_PROP(PROTECTED, long, bufferSize, HTTP_SENDBUF_SIZE);
	DCL_STATIC_PROP(PROTECTED, double, throttleDelay, 0.0);
	DCL_STATIC_PROP_N(PROTECTED, headers);
}

/* ### USERLAND ### */

/* {{{ proto static bool HttpResponse::setHeader(string name, mixed value[, bool replace = true)
 */
PHP_METHOD(HttpResponse, setHeader)
{
	zend_bool replace = 1;
	char *name;
	int name_len = 0;
	zval *value = NULL, *headers, **header;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz/!|b", &name, &name_len, &value, &replace)) {
		RETURN_FALSE;
	}
	if (!name_len) {
		http_error(HE_WARNING, HTTP_E_HEADER, "Cannot send anonymous headers");
		RETURN_FALSE;
	}

	headers = GET_STATIC_PROP(headers);

	if (Z_TYPE_P(headers) != IS_ARRAY) {
		convert_to_array(headers);
	}

	/* delete header if value == null */
	if (!value || Z_TYPE_P(value) == IS_NULL) {
		RETURN_SUCCESS(zend_hash_del(Z_ARRVAL_P(headers), name, name_len + 1));
	}

	if (Z_TYPE_P(value) != IS_STRING) {
		convert_to_string_ex(&value);
	}

	/* convert old header to an array and add new one if header exists and replace == false */
	if (replace || (SUCCESS != zend_hash_find(Z_ARRVAL_P(headers), name, name_len + 1, (void **) &header))) {
		RETURN_SUCCESS(add_assoc_stringl_ex(headers, name, name_len + 1, Z_STRVAL_P(value), Z_STRLEN_P(value), 1));
	} else {
		convert_to_array(*header);
		RETURN_SUCCESS(add_next_index_stringl(*header, Z_STRVAL_P(value), Z_STRLEN_P(value), 1));
	}
}
/* }}} */

/* {{{ proto static mixed HttpResponse::getHeader([string name])
 */
PHP_METHOD(HttpResponse, getHeader)
{
	char *name = NULL;
	int name_len = 0;
	zval *headers, **header;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &name, &name_len)) {
		RETURN_FALSE;
	}
	
	headers = GET_STATIC_PROP(headers);
	if (Z_TYPE_P(headers) != IS_ARRAY) {
		convert_to_array(headers);
	}
	
	if (!name || !name_len) {
		array_init(return_value);
		array_copy(headers, return_value);
	} else if (SUCCESS == zend_hash_find(Z_ARRVAL_P(headers), name, name_len + 1, (void **) &header)) {
		RETURN_ZVAL(*header, ZVAL_PTR_DTOR, 1);
	} else {
		RETURN_NULL();
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setCache(bool cache)
 *
 * Whether it sould be attempted to cache the entitity.
 * This will result in necessary caching headers and checks of clients
 * "If-Modified-Since" and "If-None-Match" headers.  If one of those headers
 * matches a "304 Not Modified" status code will be issued.
 *
 * NOTE: If you're using sessions, be shure that you set session.cache_limiter
 * to something more appropriate than "no-cache"!
 */
PHP_METHOD(HttpResponse, setCache)
{
	zend_bool do_cache = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &do_cache)) {
		RETURN_FALSE;
	}

	RETURN_SUCCESS(UPD_STATIC_PROP(bool, cache, do_cache));
}
/* }}} */

/* {{{ proto static bool HttpResponse::getCache()
 *
 * Get current caching setting.
 */
PHP_METHOD(HttpResponse, getCache)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_BOOL(Z_LVAL_P(GET_STATIC_PROP(cache)));
	}
}
/* }}}*/

/* {{{ proto static bool HttpResponse::setGzip(bool gzip)
 *
 * Enable on-thy-fly gzipping of the sent entity.
 */
PHP_METHOD(HttpResponse, setGzip)
{
	zend_bool do_gzip = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &do_gzip)) {
		RETURN_FALSE;
	}

	RETURN_SUCCESS(UPD_STATIC_PROP(bool, gzip, do_gzip));
}
/* }}} */

/* {{{ proto static bool HttpResponse::getGzip()
 *
 * Get current gzipping setting.
 */
PHP_METHOD(HttpResponse, getGzip)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_BOOL(Z_LVAL_P(GET_STATIC_PROP(gzip)));
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setCacheControl(string control[, long max_age = 0])
 *
 * Set a custom cache-control header, usually being "private" or "public";
 * The max_age parameter controls how long the cache entry is valid on the client side.
 */
PHP_METHOD(HttpResponse, setCacheControl)
{
	char *ccontrol, *cctl;
	int cc_len;
	long max_age = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &ccontrol, &cc_len, &max_age)) {
		RETURN_FALSE;
	}

	if (strcmp(ccontrol, "public") && strcmp(ccontrol, "private") && strcmp(ccontrol, "no-cache")) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Cache-Control '%s' doesn't match public, private or no-cache", ccontrol);
		RETURN_FALSE;
	} else {
		spprintf(&cctl, 0, "%s, must-revalidate, max_age=%ld", ccontrol, max_age);
		RETVAL_SUCCESS(UPD_STATIC_PROP(string, cacheControl, cctl));
		efree(cctl);
	}
}
/* }}} */

/* {{{ proto static string HttpResponse::getCacheControl()
 *
 * Get current Cache-Control header setting.
 */
PHP_METHOD(HttpResponse, getCacheControl)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *ccontrol = GET_STATIC_PROP(cacheControl);
		RETURN_STRINGL(Z_STRVAL_P(ccontrol), Z_STRLEN_P(ccontrol), 1);
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setContentType(string content_type)
 *
 * Set the content-type of the sent entity.
 */
PHP_METHOD(HttpResponse, setContentType)
{
	char *ctype;
	int ctype_len;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ctype_len)) {
		RETURN_FALSE;
	}

	if (!strchr(ctype, '/')) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Content type '%s' doesn't seem to contain a primary and a secondary part", ctype);
		RETURN_FALSE;
	}

	RETURN_SUCCESS(UPD_STATIC_PROP(string, contentType, ctype));
}
/* }}} */

/* {{{ proto static string HttpResponse::getContentType()
 *
 * Get current Content-Type header setting.
 */
PHP_METHOD(HttpResponse, getContentType)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *ctype = GET_STATIC_PROP(contentType);
		RETURN_STRINGL(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setContentDisposition(string filename[, bool inline = false])
 *
 * Set the Content-Disposition of the sent entity.  This setting aims to suggest
 * the receiveing user agent how to handle the sent entity;  usually the client
 * will show the user a "Save As..." popup.
 */
PHP_METHOD(HttpResponse, setContentDisposition)
{
	char *file, *cd;
	int file_len;
	zend_bool send_inline = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &file, &file_len, &send_inline)) {
		RETURN_FALSE;
	}

	spprintf(&cd, 0, "%s; filename=\"%s\"", send_inline ? "inline" : "attachment", file);
	RETVAL_SUCCESS(UPD_STATIC_PROP(string, contentDisposition, cd));
	efree(cd);
}
/* }}} */

/* {{{ proto static string HttpResponse::getContentDisposition()
 *
 * Get current Content-Disposition setting.
 */
PHP_METHOD(HttpResponse, getContentDisposition)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *cd = GET_STATIC_PROP(contentDisposition);
		RETURN_STRINGL(Z_STRVAL_P(cd), Z_STRLEN_P(cd), 1);
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setETag(string etag)
 *
 * Set a custom ETag.  Use this only if you know what you're doing.
 */
PHP_METHOD(HttpResponse, setETag)
{
	char *etag;
	int etag_len;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &etag, &etag_len)) {
		RETURN_FALSE;
	}

	RETURN_SUCCESS(UPD_STATIC_PROP(string, eTag, etag));
}
/* }}} */

/* {{{ proto static string HttpResponse::getETag()
 *
 * Get calculated or previously set custom ETag.
 */
PHP_METHOD(HttpResponse, getETag)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *etag = GET_STATIC_PROP(eTag);
		RETURN_STRINGL(Z_STRVAL_P(etag), Z_STRLEN_P(etag), 1);
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setLastModified(long timestamp)
 *
 * Set a custom Last-Modified date.
 */
PHP_METHOD(HttpResponse, setLastModified)
{
	long lm;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &lm)) {
		RETURN_FALSE;
	}
	
	RETURN_SUCCESS(UPD_STATIC_PROP(long, lastModified, lm));
}
/* }}} */

/* {{{ proto static HttpResponse::getLastModified()
 *
 * Get calculated or previously set custom Last-Modified date.
 */
PHP_METHOD(HttpResponse, getLastModified)
{
	NO_ARGS;
	
	IF_RETVAL_USED {
		RETURN_LONG(Z_LVAL_P(GET_STATIC_PROP(lastModified)));
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setThrottleDelay(double seconds)
 *
 */
PHP_METHOD(HttpResponse, setThrottleDelay)
{
	double seconds;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &seconds)) {
		RETURN_FALSE;
	}
	RETURN_SUCCESS(UPD_STATIC_PROP(double, throttleDelay, seconds));
}
/* }}} */

/* {{{ proto static double HttpResponse::getThrottleDelay()
 *
 */
PHP_METHOD(HttpResponse, getThrottleDelay)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_DOUBLE(Z_DVAL_P(GET_STATIC_PROP(throttleDelay)));
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setBufferSize(long bytes)
 *
 */
PHP_METHOD(HttpResponse, setBufferSize)
{
	long bytes;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &bytes)) {
		RETURN_FALSE;
	}
	RETURN_SUCCESS(UPD_STATIC_PROP(long, bufferSize, bytes));
}
/* }}} */

/* {{{ proto static long HttpResponse::getBufferSize()
 *
 */
PHP_METHOD(HttpResponse, getBufferSize)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_LONG(Z_LVAL_P(GET_STATIC_PROP(bufferSize)));
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setData(string data)
 *
 * Set the data to be sent.
 */
PHP_METHOD(HttpResponse, setData)
{
	zval *the_data;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &the_data)) {
		RETURN_FALSE;
	}
	if (Z_TYPE_P(the_data) != IS_STRING) {
		convert_to_string_ex(&the_data);
	}

	if (	(SUCCESS != SET_STATIC_PROP(data, the_data)) ||
			(SUCCESS != UPD_STATIC_PROP(long, mode, SEND_DATA))) {
		RETURN_FALSE;
	}
	
	if (!(Z_LVAL_P(GET_STATIC_PROP(lastModified)) > 0)) {
		UPD_STATIC_PROP(long, lastModified, http_last_modified(the_data, SEND_DATA));
	}
	if (!Z_STRLEN_P(GET_STATIC_PROP(eTag))) {
		char *etag = http_etag(Z_STRVAL_P(the_data), Z_STRLEN_P(the_data), SEND_DATA);
		UPD_STATIC_PROP(string, eTag, etag);
		efree(etag);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto static string HttpResponse::getData()
 *
 * Get the previously set data to be sent.
 */
PHP_METHOD(HttpResponse, getData)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *the_data = GET_STATIC_PROP(data);
		RETURN_STRINGL(Z_STRVAL_P(the_data), Z_STRLEN_P(the_data), 1);
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setStream(resource stream)
 *
 * Set the resource to be sent.
 */
PHP_METHOD(HttpResponse, setStream)
{
	zval *the_stream;
	php_stream *the_real_stream;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &the_stream)) {
		RETURN_FALSE;
	}
	zend_list_addref(Z_LVAL_P(the_stream));
	php_stream_from_zval(the_real_stream, &the_stream);

	if (	(SUCCESS != UPD_STATIC_PROP(long, stream, Z_LVAL_P(the_stream))) ||
			(SUCCESS != UPD_STATIC_PROP(long, mode, SEND_RSRC))) {
		RETURN_FALSE;
	}

	if (!(Z_LVAL_P(GET_STATIC_PROP(lastModified)) > 0)) {
		UPD_STATIC_PROP(long, lastModified, http_last_modified(the_real_stream, SEND_RSRC));
	}
	if (!Z_STRLEN_P(GET_STATIC_PROP(eTag))) {
		char *etag = http_etag(the_real_stream, 0, SEND_RSRC);
		UPD_STATIC_PROP(string, eTag, etag);
		efree(etag);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto static resource HttpResponse::getStream()
 *
 * Get the previously set resource to be sent.
 */
PHP_METHOD(HttpResponse, getStream)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_RESOURCE(Z_LVAL_P(GET_STATIC_PROP(stream)));
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::setFile(string file)
 *
 * Set the file to be sent.
 */
PHP_METHOD(HttpResponse, setFile)
{
	char *the_file;
	int file_len;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &the_file, &file_len)) {
		RETURN_FALSE;
	}
	
	if (	(SUCCESS != UPD_STATIC_PROP(string, file, the_file)) ||
			(SUCCESS != UPD_STATIC_PROP(long, mode, -1))) {
		RETURN_FALSE;
	}

	if (!(Z_LVAL_P(GET_STATIC_PROP(lastModified)))) {
		UPD_STATIC_PROP(long, lastModified, http_last_modified(the_file, -1));
	}
	if (!Z_STRLEN_P(GET_STATIC_PROP(eTag))) {
		char *etag = http_etag(the_file, 0, -1);
		UPD_STATIC_PROP(string, eTag, etag);
		efree(etag);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto static string HttpResponse::getFile()
 *
 * Get the previously set file to be sent.
 */
PHP_METHOD(HttpResponse, getFile)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *the_file = GET_STATIC_PROP(file);
		RETURN_STRINGL(Z_STRVAL_P(the_file), Z_STRLEN_P(the_file), 1);
	}
}
/* }}} */

/* {{{ proto static bool HttpResponse::send([bool clean_ob = true])
 *
 * Finally send the entity.
 *
 * Example:
 * <pre>
 * <?php
 * HttpResponse::setCache(true);
 * HttpResponse::setContentType('application/pdf');
 * HttpResponse::setContentDisposition("$user.pdf", false);
 * HttpResponse::setFile('sheet.pdf');
 * HttpResponse::send();
 * ?>
 * </pre>
 */
PHP_METHOD(HttpResponse, send)
{
	zval *sent, *headers;
	zend_bool clean_ob = 1;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &clean_ob)) {
		RETURN_FALSE;
	}
	if (SG(headers_sent)) {
		http_error(HE_WARNING, HTTP_E_RESPONSE, "Cannot send HttpResponse, headers have already been sent");
		RETURN_FALSE;
	}

	sent = GET_STATIC_PROP(sent);
	if (Z_LVAL_P(sent)) {
		http_error(HE_WARNING, HTTP_E_RESPONSE, "Cannot send HttpResponse, response has already been sent");
		RETURN_FALSE;
	} else {
		Z_LVAL_P(sent) = 1;
	}

	/* capture mode */
	if (Z_BVAL_P(GET_STATIC_PROP(catch))) {
		zval the_data;

		INIT_PZVAL(&the_data);
		php_ob_get_buffer(&the_data TSRMLS_CC);
		SET_STATIC_PROP(data, &the_data);
		ZVAL_LONG(GET_STATIC_PROP(mode), SEND_DATA);

		if (!Z_STRLEN_P(GET_STATIC_PROP(eTag))) {
			char *etag = http_etag(Z_STRVAL(the_data), Z_STRLEN(the_data), SEND_DATA);
			UPD_STATIC_PROP(string, eTag, etag);
			efree(etag);
		}
		zval_dtor(&the_data);

		clean_ob = 1;
	}

	if (clean_ob) {
		/* interrupt on-the-fly etag generation */
		HTTP_G(etag).started = 0;
		/* discard previous output buffers */
		php_end_ob_buffers(0 TSRMLS_CC);
	}

	/* custom headers */
	headers = GET_STATIC_PROP(headers);
	if (Z_TYPE_P(headers) == IS_ARRAY) {
		char *name = NULL;
		ulong idx = 0;
		zval **value;

		FOREACH_KEYVAL(headers, name, idx, value) {
			if (name) {
				if (Z_TYPE_PP(value) == IS_ARRAY) {
					zend_bool first = 1;
					zval **data;

					FOREACH_VAL(*value, data) {
						http_send_header_ex(name, strlen(name), Z_STRVAL_PP(data), Z_STRLEN_PP(data), first, NULL);
						first = 0;
					}
				} else {
					http_send_header_ex(name, strlen(name), Z_STRVAL_PP(value), Z_STRLEN_PP(value), 1, NULL);
				}
				name = NULL;
			}
		}
	}

	/* gzip */
	if (Z_LVAL_P(GET_STATIC_PROP(gzip))) {
		php_start_ob_buffer_named("ob_gzhandler", 0, 0 TSRMLS_CC);
	} else {
		php_start_ob_buffer(NULL, 0, 0 TSRMLS_CC);
	}

	/* caching */
	if (Z_LVAL_P(GET_STATIC_PROP(cache))) {
		zval *cctl, *etag, *lmod;

		etag = GET_STATIC_PROP(eTag);
		lmod = GET_STATIC_PROP(lastModified);
		cctl = GET_STATIC_PROP(cacheControl);

		http_cache_etag(Z_STRVAL_P(etag), Z_STRLEN_P(etag), Z_STRVAL_P(cctl), Z_STRLEN_P(cctl));
		http_cache_last_modified(Z_LVAL_P(lmod), Z_LVAL_P(lmod) ? Z_LVAL_P(lmod) : time(NULL), Z_STRVAL_P(cctl), Z_STRLEN_P(cctl));
	}

	/* content type */
	{
		zval *ctype = GET_STATIC_PROP(contentType);
		if (Z_STRLEN_P(ctype)) {
			http_send_content_type(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype));
		} else {
			char *ctypes = INI_STR("default_mimetype");
			size_t ctlen = ctypes ? strlen(ctypes) : 0;

			if (ctlen) {
				http_send_content_type(ctypes, ctlen);
			} else {
				http_send_content_type("application/x-octetstream", lenof("application/x-octetstream"));
			}
		}
	}

	/* content disposition */
	{
		zval *cd = GET_STATIC_PROP(contentDisposition);
		if (Z_STRLEN_P(cd)) {
			http_send_header_ex("Content-Disposition", lenof("Content-Disposition"), Z_STRVAL_P(cd), Z_STRLEN_P(cd), 1, NULL);
		}
	}

	/* throttling */
	{
		HTTP_G(send).buffer_size    = Z_LVAL_P(GET_STATIC_PROP(bufferSize));
		HTTP_G(send).throttle_delay = Z_DVAL_P(GET_STATIC_PROP(throttleDelay));
	}

	/* send */
	{
		switch (Z_LVAL_P(GET_STATIC_PROP(mode)))
		{
			case SEND_DATA:
			{
				zval *zdata = GET_STATIC_PROP(data);
				RETURN_SUCCESS(http_send_data(Z_STRVAL_P(zdata), Z_STRLEN_P(zdata)));
			}

			case SEND_RSRC:
			{
				php_stream *the_real_stream;
				zval *the_stream = GET_STATIC_PROP(stream);
				the_stream->type = IS_RESOURCE;
				php_stream_from_zval(the_real_stream, &the_stream);
				RETURN_SUCCESS(http_send_stream(the_real_stream));
			}

			default:
			{
				RETURN_SUCCESS(http_send_file(Z_STRVAL_P(GET_STATIC_PROP(file))));
			}
		}
	}
}
/* }}} */

/* {{{ proto static void HttpResponse::capture()
 *
 * Capture script output.
 *
 * Example:
 * <pre>
 * <?php
 * HttpResponse::setCache(true);
 * HttpResponse::capture();
 * // script follows
 * // note that you need to call
 * HttpResponse::send();
 * // at the end of the script unless 
 * // you use PHP-5.1 or greater
 * ?>
 * </pre>
 */
PHP_METHOD(HttpResponse, capture)
{
	zval do_catch;

	NO_ARGS;

	INIT_PZVAL(&do_catch);
	ZVAL_LONG(&do_catch, 1);

	SET_STATIC_PROP(catch, &do_catch);

	php_end_ob_buffers(0 TSRMLS_CC);
	php_start_ob_buffer(NULL, 0, 0 TSRMLS_CC);

#if (PHP_MAJOR_VERSION > 5) || (PHP_MINOR_VERSION > 0)
	/* register shutdown function --
		messing around with ob and headers only works in PHP-5.1 or greater */
	{
		zval func, retval, arg, *argp[1];

		INIT_PZVAL(&arg);
		INIT_PZVAL(&func);
		INIT_PZVAL(&retval);
		ZVAL_STRINGL(&func, "register_shutdown_function", lenof("register_shutdown_function"), 0);

		array_init(&arg);
		add_next_index_stringl(&arg, "HttpResponse", lenof("HttpResponse"), 1);
		add_next_index_stringl(&arg, "send", lenof("send"), 1);
		argp[0] = &arg;
		call_user_function(EG(function_table), NULL, &func, &retval, 1, argp TSRMLS_CC);
		zval_dtor(&arg);
	}
#endif
}
/* }}} */

#endif /* ZEND_ENGINE_2 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

