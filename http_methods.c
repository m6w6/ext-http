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

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_cache_api.h"
#include "php_http_request_api.h"
#include "php_http_request_pool_api.h"
#include "php_http_date_api.h"
#include "php_http_headers_api.h"
#include "php_http_message_api.h"
#include "php_http_send_api.h"
#include "php_http_url_api.h"

#include "php_http_message_object.h"
#include "php_http_response_object.h"
#include "php_http_request_object.h"
#include "php_http_requestpool_object.h"
#include "php_http_exception_object.h"

#ifdef ZEND_ENGINE_2

#include "missing.h"

ZEND_EXTERN_MODULE_GLOBALS(http)

/* {{{ HttpResponse */

/* {{{ proto void HttpResponse::__construct(bool cache, bool gzip)
 *
 * Instantiates a new HttpResponse object, which can be used to send
 * any data/resource/file to an HTTP client with caching and multiple
 * ranges/resuming support.
 *
 * NOTE: GZIPping is not implemented yet.
 */
PHP_METHOD(HttpResponse, __construct)
{
	zend_bool do_cache = 0, do_gzip = 0;
	getObject(http_response_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|bb", &do_cache, &do_gzip)) {
		UPD_PROP(obj, long, cache, do_cache);
		UPD_PROP(obj, long, gzip, do_gzip);
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto bool HttpResponse::setCache(bool cache)
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
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &do_cache)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, cache, do_cache);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HttpResponse::getCache()
 *
 * Get current caching setting.
 */
PHP_METHOD(HttpResponse, getCache)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *do_cache = NULL;
		getObject(http_response_object, obj);

		do_cache = GET_PROP(obj, cache);
		RETURN_BOOL(Z_LVAL_P(do_cache));
	}
}
/* }}}*/

/* {{{ proto bool HttpResponse::setGzip(bool gzip)
 *
 * Enable on-thy-fly gzipping of the sent entity. NOT IMPLEMENTED YET.
 */
PHP_METHOD(HttpResponse, setGzip)
{
	zend_bool do_gzip = 0;
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &do_gzip)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, gzip, do_gzip);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HttpResponse::getGzip()
 *
 * Get current gzipping setting.
 */
PHP_METHOD(HttpResponse, getGzip)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *do_gzip;
		getObject(http_response_object, obj);

		do_gzip = GET_PROP(obj, gzip);
		RETURN_BOOL(Z_LVAL_P(do_gzip));
	}
}
/* }}} */

/* {{{ proto bool HttpResponse::setCacheControl(string control[, bool raw = false])
 *
 * Set a custom cache-control header, usually being "private" or "public";  if
 * $raw is set to true the header will be sent as-is.
 */
PHP_METHOD(HttpResponse, setCacheControl)
{
	char *ccontrol;
	int cc_len;
	zend_bool raw = 0;
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &ccontrol, &cc_len, &raw)) {
		RETURN_FALSE;
	}

	if ((!raw) && (strcmp(ccontrol, "public") && strcmp(ccontrol, "private") && strcmp(ccontrol, "no-cache"))) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Cache-Control '%s' doesn't match public, private or no-cache", ccontrol);
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, raw_cache_header, raw);
	UPD_PROP(obj, string, cacheControl, ccontrol);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpResponse::getCacheControl()
 *
 * Get current Cache-Control header setting.
 */
PHP_METHOD(HttpResponse, getCacheControl)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *ccontrol;
		getObject(http_response_object, obj);

		ccontrol = GET_PROP(obj, cacheControl);
		RETURN_STRINGL(Z_STRVAL_P(ccontrol), Z_STRLEN_P(ccontrol), 1);
	}
}
/* }}} */

/* {{{ proto bool HttpResponse::setContentType(string content_type)
 *
 * Set the content-type of the sent entity.
 */
PHP_METHOD(HttpResponse, setContentType)
{
	char *ctype;
	int ctype_len;
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ctype_len)) {
		RETURN_FALSE;
	}

	if (!strchr(ctype, '/')) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Content type '%s' doesn't seem to contain a primary and a secondary part", ctype);
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, contentType, ctype);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpResponse::getContentType()
 *
 * Get current Content-Type header setting.
 */
PHP_METHOD(HttpResponse, getContentType)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *ctype;
		getObject(http_response_object, obj);

		ctype = GET_PROP(obj, contentType);
		RETURN_STRINGL(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
	}
}
/* }}} */

/* {{{ proto bool HttpResponse::setContentDisposition(string filename[, bool inline = false])
 *
 * Set the Content-Disposition of the sent entity.  This setting aims to suggest
 * the receiveing user agent how to handle the sent entity;  usually the client
 * will show the user a "Save As..." popup.
 */
PHP_METHOD(HttpResponse, setContentDisposition)
{
	char *file;
	int file_len;
	zend_bool is_inline = 0;
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &file, &file_len, &is_inline)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, dispoFile, file);
	UPD_PROP(obj, long, dispoInline, is_inline);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HttpResponse::getContentDisposition()
 *
 * Get current Content-Disposition setting.
 * Will return an associative array like:
 * <pre>
 * array(
 *     'filename' => 'foo.bar',
 *     'inline'   => false
 * )
 * </pre>
 */
PHP_METHOD(HttpResponse, getContentDisposition)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *file, *is_inline;
		getObject(http_response_object, obj);

		file = GET_PROP(obj, dispoFile);
		is_inline = GET_PROP(obj, dispoInline);

		array_init(return_value);
		add_assoc_stringl(return_value, "filename", Z_STRVAL_P(file), Z_STRLEN_P(file), 1);
		add_assoc_bool(return_value, "inline", Z_LVAL_P(is_inline));
	}
}
/* }}} */

/* {{{ proto bool HttpResponse::setETag(string etag)
 *
 * Set a custom ETag.  Use this only if you know what you're doing.
 */
PHP_METHOD(HttpResponse, setETag)
{
	char *etag;
	int etag_len;
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &etag, &etag_len)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, eTag, etag);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpResponse::getETag()
 *
 * Get the previously set custom ETag.
 */
PHP_METHOD(HttpResponse, getETag)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *etag;
		getObject(http_response_object, obj);

		etag = GET_PROP(obj, eTag);
		RETURN_STRINGL(Z_STRVAL_P(etag), Z_STRLEN_P(etag), 1);
	}
}
/* }}} */

/* {{{ proto void HttpResponse::setThrottleDelay(double seconds)
 *
 */
PHP_METHOD(HttpResponse, setThrottleDelay)
{
	double seconds;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d", &seconds)) {
		getObject(http_response_object, obj);

		UPD_PROP(obj, double, throttleDelay, seconds);
	}
}
/* }}} */

/* {{{ proto double HttpResponse::getThrottleDelay()
 *
 */
PHP_METHOD(HttpResponse, getThrottleDelay)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *seconds;
		getObject(http_response_object, obj);

		seconds = GET_PROP(obj, throttleDelay);
		RETURN_DOUBLE(Z_DVAL_P(seconds));
	}
}
/* }}} */

/* {{{ proto void HttpResponse::setSendBuffersize(long bytes)
 *
 */
PHP_METHOD(HttpResponse, setSendBuffersize)
{
	long bytes;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &bytes)) {
		getObject(http_response_object, obj);

		UPD_PROP(obj, long, sendBuffersize, bytes);
	}
}
/* }}} */

/* {{{ proto long HttpResponse::getSendBuffersize()
 *
 */
PHP_METHOD(HttpResponse, getSendBuffersize)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *bytes;
		getObject(http_response_object, obj);

		bytes = GET_PROP(obj, sendBuffersize);
		RETURN_LONG(Z_LVAL_P(bytes));
	}
}
/* }}} */

/* {{{ proto bool HttpResponse::setData(string data)
 *
 * Set the data to be sent.
 */
PHP_METHOD(HttpResponse, setData)
{
	zval *the_data;
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &the_data)) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&the_data);
	SET_PROP(obj, data, the_data);
	UPD_PROP(obj, long, lastModified, http_last_modified(the_data, SEND_DATA));
	UPD_PROP(obj, long, send_mode, SEND_DATA);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpResponse::getData()
 *
 * Get the previously set data to be sent.
 */
PHP_METHOD(HttpResponse, getData)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *the_data;
		getObject(http_response_object, obj);

		the_data = GET_PROP(obj, data);
		RETURN_STRINGL(Z_STRVAL_P(the_data), Z_STRLEN_P(the_data), 1);
	}
}
/* }}} */

/* {{{ proto bool HttpResponse::setStream(resource stream)
 *
 * Set the resource to be sent.
 */
PHP_METHOD(HttpResponse, setStream)
{
	zval *the_stream;
	php_stream *the_real_stream;
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &the_stream)) {
		RETURN_FALSE;
	}

	php_stream_from_zval(the_real_stream, &the_stream);

	SET_PROP(obj, stream, the_stream);
	UPD_PROP(obj, long, lastModified, http_last_modified(the_real_stream, SEND_RSRC));
	UPD_PROP(obj, long, send_mode, SEND_RSRC);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto resource HttpResponse::getStream()
 *
 * Get the previously set resource to be sent.
 */
PHP_METHOD(HttpResponse, getStream)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *the_stream;
		getObject(http_response_object, obj);

		the_stream = GET_PROP(obj, stream);
		RETURN_RESOURCE(Z_LVAL_P(the_stream));
	}
}
/* }}} */

/* {{{ proto bool HttpResponse::setFile(string file)
 *
 * Set the file to be sent.
 */
PHP_METHOD(HttpResponse, setFile)
{
	zval *the_file;
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &the_file)) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&the_file);

	UPD_PROP(obj, string, file, Z_STRVAL_P(the_file));
	UPD_PROP(obj, long, lastModified, http_last_modified(the_file, -1));
	UPD_PROP(obj, long, send_mode, -1);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpResponse::getFile()
 *
 * Get the previously set file to be sent.
 */
PHP_METHOD(HttpResponse, getFile)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *the_file;
		getObject(http_response_object, obj);

		the_file = GET_PROP(obj, file);
		RETURN_STRINGL(Z_STRVAL_P(the_file), Z_STRLEN_P(the_file), 1);
	}
}
/* }}} */

/* {{{ proto bool HttpResponse::send([bool clean_ob = true])
 *
 * Finally send the entity.
 *
 * Example:
 * <pre>
 * <?php
 * $r = new HttpResponse(true);
 * $r->setFile('../hidden/contract.pdf');
 * $r->setContentType('application/pdf');
 * $r->send();
 * ?>
 * </pre>
 *
 */
PHP_METHOD(HttpResponse, send)
{
	zend_bool clean_ob = 1;
	zval *do_cache, *do_gzip;
	getObject(http_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &clean_ob)) {
		RETURN_FALSE;
	}

	do_cache = GET_PROP(obj, cache);
	do_gzip  = GET_PROP(obj, gzip);

	if (clean_ob) {
		/* interrupt on-the-fly etag generation */
		HTTP_G(etag).started = 0;
		/* discard previous output buffers */
		php_end_ob_buffers(0 TSRMLS_CC);
	}

	/* gzip */
	if (Z_LVAL_P(do_gzip)) {
		php_start_ob_buffer_named("ob_gzhandler", 0, 1 TSRMLS_CC);
	}

	/* caching */
	if (Z_LVAL_P(do_cache)) {
		char *cc_hdr;
		int cc_len;
		zval *cctrl, *etag, *lmod, *ccraw;

		etag  = GET_PROP(obj, eTag);
		lmod  = GET_PROP(obj, lastModified);
		cctrl = GET_PROP(obj, cacheControl);
		ccraw = GET_PROP(obj, raw_cache_header);

		if (Z_LVAL_P(ccraw)) {
			cc_hdr = Z_STRVAL_P(cctrl);
			cc_len = Z_STRLEN_P(cctrl);
		} else {
			char cc_header[42] = {0};
			sprintf(cc_header, "%s, must-revalidate, max-age=0", Z_STRVAL_P(cctrl));
			cc_hdr = cc_header;
			cc_len = Z_STRLEN_P(cctrl) + lenof(", must-revalidate, max-age=0");
		}

		http_cache_etag(Z_STRVAL_P(etag), Z_STRLEN_P(etag), cc_hdr, cc_len);
		http_cache_last_modified(Z_LVAL_P(lmod), Z_LVAL_P(lmod) ? Z_LVAL_P(lmod) : time(NULL), cc_hdr, cc_len);
	}

	/* content type */
	{
		zval *ctype = GET_PROP(obj, contentType);
		if (Z_STRLEN_P(ctype)) {
			http_send_content_type(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype));
		} else {
			http_send_content_type("application/x-octetstream", lenof("application/x-octetstream"));
		}
	}

	/* content disposition */
	{
		zval *dispo_file = GET_PROP(obj, dispoFile);
		if (Z_STRLEN_P(dispo_file)) {
			zval *dispo_inline = GET_PROP(obj, dispoInline);
			http_send_content_disposition(Z_STRVAL_P(dispo_file), Z_STRLEN_P(dispo_file), (zend_bool) Z_LVAL_P(dispo_inline));
		}
	}

	/* throttling */
	{
		zval *send_buffersize, *throttle_delay;
		send_buffersize = GET_PROP(obj, sendBuffersize);
		throttle_delay  = GET_PROP(obj, throttleDelay);
		HTTP_G(send).buffer_size    = Z_LVAL_P(send_buffersize);
		HTTP_G(send).throttle_delay = Z_DVAL_P(throttle_delay);
	}

	/* send */
	{
		zval *send_mode = GET_PROP(obj, send_mode);
		switch (Z_LVAL_P(send_mode))
		{
			case SEND_DATA:
			{
				zval *zdata = GET_PROP(obj, data);
				RETURN_SUCCESS(http_send_data(Z_STRVAL_P(zdata), Z_STRLEN_P(zdata)));
			}

			case SEND_RSRC:
			{
				php_stream *the_real_stream;
				zval *the_stream = GET_PROP(obj, stream);
				php_stream_from_zval(the_real_stream, &the_stream);
				RETURN_SUCCESS(http_send_stream(the_real_stream));
			}

			default:
			{
				zval *zfile = GET_PROP(obj, file);
				RETURN_SUCCESS(http_send_file(Z_STRVAL_P(zfile)));
			}
		}
	}
}
/* }}} */
/* }}} */

/* {{{ HttpMessage */

/* {{{ proto void HttpMessage::__construct([string message])
 *
 * Instantiate a new HttpMessage object.
 */
PHP_METHOD(HttpMessage, __construct)
{
	char *message = NULL;
	int length = 0;
	getObject(http_message_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &message, &length) && message && length) {
		if (obj->message = http_message_parse(message, length)) {
			if (obj->message->parent) {
				obj->parent = http_message_object_from_msg(obj->message->parent);
			}
		}
	} else if (!obj->message) {
		obj->message = http_message_new();
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto static HttpMessage HttpMessage::fromString(string raw_message)
 *
 * Create an HttpMessage object from a string.
 */
PHP_METHOD(HttpMessage, fromString)
{
	char *string = NULL;
	int length = 0;
	http_message *msg = NULL;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &string, &length)) {
		RETURN_NULL();
	}

	if (!(msg = http_message_parse(string, length))) {
		RETURN_NULL();
	}

	Z_TYPE_P(return_value) = IS_OBJECT;
	return_value->value.obj = http_message_object_from_msg(msg);
}
/* }}} */

/* {{{ proto string HttpMessage::getBody()
 *
 * Get the body of the parsed Message.
 */
PHP_METHOD(HttpMessage, getBody)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		RETURN_PHPSTR(&obj->message->body, PHPSTR_FREE_NOT, 1);
	}
}
/* }}} */

/* {{{ proto array HttpMessage::getHeaders()
 *
 * Get Message Headers.
 */
PHP_METHOD(HttpMessage, getHeaders)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval headers;
		getObject(http_message_object, obj);

		Z_ARRVAL(headers) = &obj->message->hdrs;
		array_init(return_value);
		array_copy(&headers, return_value);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setHeaders(array headers)
 *
 * Sets new headers.
 */
PHP_METHOD(HttpMessage, setHeaders)
{
	zval *new_headers, old_headers;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &new_headers)) {
		return;
	}

	zend_hash_clean(&obj->message->hdrs);
	Z_ARRVAL(old_headers) = &obj->message->hdrs;
	array_copy(new_headers, &old_headers);
}
/* }}} */

/* {{{ proto void HttpMessage::addHeaders(array headers[, bool append = false])
 *
 * Add headers. If append is true, headers with the same name will be separated, else overwritten.
 */
PHP_METHOD(HttpMessage, addHeaders)
{
	zval old_headers, *new_headers;
	zend_bool append = 0;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|b", &new_headers, &append)) {
		return;
	}

	Z_ARRVAL(old_headers) = &obj->message->hdrs;
	if (append) {
		array_append(new_headers, &old_headers);
	} else {
		array_merge(new_headers, &old_headers);
	}
}
/* }}} */

/* {{{ proto long HttpMessage::getType()
 *
 * Get Message Type. (HTTP_MSG_NONE|HTTP_MSG_REQUEST|HTTP_MSG_RESPONSE)
 */
PHP_METHOD(HttpMessage, getType)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		RETURN_LONG(obj->message->type);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setType(long type)
 *
 * Set Message Type. (HTTP_MSG_NONE|HTTP_MSG_REQUEST|HTTP_MSG_RESPONSE)
 */
PHP_METHOD(HttpMessage, setType)
{
	long type;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type)) {
		return;
	}
	http_message_set_type(obj->message, type);
}
/* }}} */

/* {{{ proto long HttpMessage::getResponseCode()
 *
 * Get the Response Code of the Message.
 */
PHP_METHOD(HttpMessage, getResponseCode)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);

		if (!HTTP_MSG_TYPE(RESPONSE, obj->message)) {
			http_error(E_NOTICE, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_RESPONSE");
			RETURN_NULL();
		}

		RETURN_LONG(obj->message->info.response.code);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setResponseCode(long code)
 *
 * Set the response code of an HTTP Response Message.
 * Returns false if the Message is not of type HTTP_MSG_RESPONSE,
 * or if the response code is out of range (100-510).
 */
PHP_METHOD(HttpMessage, setResponseCode)
{
	long code;
	getObject(http_message_object, obj);

	if (!HTTP_MSG_TYPE(RESPONSE, obj->message)) {
		http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_RESPONSE");
		RETURN_FALSE;
	}

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &code)) {
		RETURN_FALSE;
	}
	if (code < 100 || code > 510) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Invalid response code (100-510): %ld", code);
		RETURN_FALSE;
	}

	obj->message->info.response.code = code;
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getRequestMethod()
 *
 * Get the Request Method of the Message.
 * Returns false if the Message is not of type HTTP_MSG_REQUEST.
 */
PHP_METHOD(HttpMessage, getRequestMethod)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);

		if (!HTTP_MSG_TYPE(REQUEST, obj->message)) {
			http_error(E_NOTICE, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_REQUEST");
			RETURN_NULL();
		}

		RETURN_STRING(obj->message->info.request.method, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setRequestMethod(string method)
 *
 * Set the Request Method of the HTTP Message.
 * Returns false if the Message is not of type HTTP_MSG_REQUEST.
 */
PHP_METHOD(HttpMessage, setRequestMethod)
{
	char *method;
	int method_len;
	getObject(http_message_object, obj);

	if (!HTTP_MSG_TYPE(REQUEST, obj->message)) {
		http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_REQUEST");
		RETURN_FALSE;
	}

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &method, &method_len)) {
		RETURN_FALSE;
	}
	if (method_len < 1) {
		http_error(E_WARNING, HTTP_E_PARAM, "Cannot set HttpMessage::requestMethod to an empty string");
		RETURN_FALSE;
	}
	if (SUCCESS != http_check_method(method)) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Unkown request method: %s", method);
		RETURN_FALSE;
	}

	STR_SET(obj->message->info.request.method, estrndup(method, method_len));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getRequestUri()
 *
 * Get the Request URI of the Message.
 */
PHP_METHOD(HttpMessage, getRequestUri)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);

		if (!HTTP_MSG_TYPE(REQUEST, obj->message)) {
			http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_REQUEST");
			RETURN_NULL();
		}

		RETURN_STRING(obj->message->info.request.URI, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setRequestUri(string URI)
 *
 * Set the Request URI of the HTTP Message.
 * Returns false if the Message is not of type HTTP_MSG_REQUEST,
 * or if paramtere URI was empty.
 */
PHP_METHOD(HttpMessage, setRequestUri)
{
	char *URI;
	int URIlen;
	getObject(http_message_object, obj);

	if (!HTTP_MSG_TYPE(REQUEST, obj->message)) {
		http_error(E_WARNING, HTTP_E_MSG, "HttpMessage is not of type HTTP_MSG_REQUEST");
		RETURN_FALSE;
	}
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &URI, &URIlen)) {
		RETURN_FALSE;
	}
	if (URIlen < 1) {
		http_error(E_WARNING, HTTP_E_PARAM, "Cannot set HttpMessage::requestUri to an empty string");
		RETURN_FALSE;
	}

	STR_SET(obj->message->info.request.URI, estrndup(URI, URIlen));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getHttpVersion()
 *
 * Get the HTTP Protocol Version of the Message.
 */
PHP_METHOD(HttpMessage, getHttpVersion)
{
	NO_ARGS;

	IF_RETVAL_USED {
		char ver[4] = {0};
		float version;
		getObject(http_message_object, obj);

		switch (obj->message->type)
		{
			case HTTP_MSG_RESPONSE:
				version = obj->message->info.response.http_version;
			break;

			case HTTP_MSG_REQUEST:
				version = obj->message->info.request.http_version;
			break;

			case HTTP_MSG_NONE:
			default:
				RETURN_NULL();
		}
		sprintf(ver, "%1.1f", version);
		RETURN_STRINGL(ver, 3, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setHttpVersion(string version)
 *
 * Set the HTTP Protocol version of the Message.
 * Returns false if version is invalid (1.0 and 1.1).
 */
PHP_METHOD(HttpMessage, setHttpVersion)
{
	char v[4];
	zval *zv;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &zv)) {
		return;
	}

	if (HTTP_MSG_TYPE(NONE, obj->message)) {
		http_error(E_WARNING, HTTP_E_MSG, "Message is neither of type HTTP_MSG_RESPONSE nor HTTP_MSG_REQUEST");
		RETURN_FALSE;
	}

	convert_to_double(zv);
	sprintf(v, "%1.1f", Z_DVAL_P(zv));
	if (strcmp(v, "1.0") && strcmp(v, "1.1")) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Invalid HTTP protocol version (1.0 or 1.1): %s", v);
		RETURN_FALSE;
	}

	if (HTTP_MSG_TYPE(RESPONSE, obj->message)) {
		obj->message->info.response.http_version = (float) Z_DVAL_P(zv);
	} else {
		obj->message->info.request.http_version = (float) Z_DVAL_P(zv);
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::getParentMessage()
 *
 * Get parent Message.
 */
PHP_METHOD(HttpMessage, getParentMessage)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);

		if (obj->message->parent) {
			RETVAL_OBJVAL(obj->parent);
		} else {
			RETVAL_NULL();
		}
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::send()
 *
 * Send the Message according to its type as Response or Request.
 */
PHP_METHOD(HttpMessage, send)
{
	getObject(http_message_object, obj);

	NO_ARGS;

	RETURN_SUCCESS(http_message_send(obj->message));
}
/* }}} */

/* {{{ proto string HttpMessage::toString([bool include_parent = true])
 *
 * Get the string representation of the Message.
 */
PHP_METHOD(HttpMessage, toString)
{
	IF_RETVAL_USED {
		char *string;
		size_t length;
		zend_bool include_parent = 1;
		getObject(http_message_object, obj);

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &include_parent)) {
			RETURN_FALSE;
		}

		if (include_parent) {
			http_message_serialize(obj->message, &string, &length);
		} else {
			http_message_tostring(obj->message, &string, &length);
		}
		RETURN_STRINGL(string, length, 0);
	}
}
/* }}} */

/* }}} */

#ifdef HTTP_HAVE_CURL
/* {{{ HttpRequest */

/* {{{ proto void HttpRequest::__construct([string url[, long request_method = HTTP_GET]])
 *
 * Instantiate a new HttpRequest object which can be used to issue HEAD, GET
 * and POST (including posting files) HTTP requests.
 */
PHP_METHOD(HttpRequest, __construct)
{
	char *URL = NULL;
	int URL_len;
	long meth = -1;
	getObject(http_request_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sl", &URL, &URL_len, &meth)) {
		INIT_PARR(obj, options);
		INIT_PARR(obj, responseInfo);
		INIT_PARR(obj, responseData);
		INIT_PARR(obj, postFields);
		INIT_PARR(obj, postFiles);

		if (URL) {
			UPD_PROP(obj, string, url, URL);
		}
		if (meth > -1) {
			UPD_PROP(obj, long, method, meth);
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto void HttpRequest::__destruct()
 *
 * Destroys the HttpRequest object.
 */
PHP_METHOD(HttpRequest, __destruct)
{
	getObject(http_request_object, obj);

	NO_ARGS;

	FREE_PARR(obj, options);
	FREE_PARR(obj, responseInfo);
	FREE_PARR(obj, responseData);
	FREE_PARR(obj, postFields);
	FREE_PARR(obj, postFiles);
}
/* }}} */

/* {{{ proto bool HttpRequest::setOptions(array options)
 *
 * Set the request options to use.  See http_get() for a full list of available options.
 */
PHP_METHOD(HttpRequest, setOptions)
{
	char *key = NULL;
	ulong idx = 0;
	zval *opts, *old_opts, **opt;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &opts)) {
		RETURN_FALSE;
	}

	old_opts = GET_PROP(obj, options);

	/* headers and cookies need extra attention -- thus cannot use array_merge() directly */
	FOREACH_KEYVAL(opts, key, idx, opt) {
		if (key) {
			if (!strcmp(key, "headers")) {
				zval **headers;
				if (SUCCESS == zend_hash_find(Z_ARRVAL_P(old_opts), "headers", sizeof("headers"), (void **) &headers)) {
					array_merge(*opt, *headers);
					continue;
				}
			} else if (!strcmp(key, "cookies")) {
				zval **cookies;
				if (SUCCESS == zend_hash_find(Z_ARRVAL_P(old_opts), "cookies", sizeof("cookies"), (void **) &cookies)) {
					array_merge(*opt, *cookies);
					continue;
				}
			}
			zval_add_ref(opt);
			add_assoc_zval(old_opts, key, *opt);

			/* reset */
			key = NULL;
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HttpRequest::getOptions()
 *
 * Get current set options.
 */
PHP_METHOD(HttpRequest, getOptions)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *opts;
		getObject(http_request_object, obj);

		opts = GET_PROP(obj, options);
		array_init(return_value);
		array_copy(opts, return_value);
	}
}
/* }}} */

/* {{{ proto void HttpRequest::unsetOptions()
 *
 * Unset all options/headers/cookies.
 */
PHP_METHOD(HttpRequest, unsetOptions)
{
	getObject(http_request_object, obj);

	NO_ARGS;

	FREE_PARR(obj, options);
	INIT_PARR(obj, options);
}
/* }}} */

/* {{{ proto bool HttpRequest::setSslOptions(array options)
 *
 * Set additional SSL options.
 */
PHP_METHOD(HttpRequest, setSslOptions)
{
	zval *opts, *old_opts, **ssl_options;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &opts)) {
		RETURN_FALSE;
	}

	old_opts = GET_PROP(obj, options);

	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(old_opts), "ssl", sizeof("ssl"), (void **) &ssl_options)) {
		array_merge(opts, *ssl_options);
	} else {
		zval_add_ref(&opts);
		add_assoc_zval(old_opts, "ssl", opts);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HttpRequest::getSslOtpions()
 *
 * Get previously set SSL options.
 */
PHP_METHOD(HttpRequest, getSslOptions)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *opts, **ssl_options;
		getObject(http_request_object, obj);

		opts = GET_PROP(obj, options);

		array_init(return_value);

		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), "ssl", sizeof("ssl"), (void **) &ssl_options)) {
			array_copy(*ssl_options, return_value);
		}
	}
}
/* }}} */

/* {{{ proto void HttpRequest::unsetSslOptions()
 *
 * Unset previously set SSL options.
 */
PHP_METHOD(HttpRequest, unsetSslOptions)
{
	zval *opts;
	getObject(http_request_object, obj);

	NO_ARGS;

	opts = GET_PROP(obj, options);
	zend_hash_del(Z_ARRVAL_P(opts), "ssl", sizeof("ssl"));
}
/* }}} */

/* {{{ proto bool HttpRequest::addHeaders(array headers)
 *
 * Add request header name/value pairs.
 */
PHP_METHOD(HttpRequest, addHeaders)
{
	zval *opts, **headers, *new_headers;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &new_headers)) {
		RETURN_FALSE;
	}

	opts = GET_PROP(obj, options);

	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), "headers", sizeof("headers"), (void **) &headers)) {
		array_merge(new_headers, *headers);
	} else {
		zval_add_ref(&new_headers);
		add_assoc_zval(opts, "headers", new_headers);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HttpRequest::getHeaders()
 *
 * Get previously set request headers.
 */
PHP_METHOD(HttpRequest, getHeaders)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *opts, **headers;
		getObject(http_request_object, obj);

		opts = GET_PROP(obj, options);

		array_init(return_value);

		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), "headers", sizeof("headers"), (void **) &headers)) {
			array_copy(*headers, return_value);
		}
	}
}
/* }}} */

/* {{{ proto void HttpRequest::unsetHeaders()
 *
 * Unset previously set request headers.
 */
PHP_METHOD(HttpRequest, unsetHeaders)
{
	zval *opts;
	getObject(http_request_object, obj);

	NO_ARGS;

	opts = GET_PROP(obj, options);
	zend_hash_del(Z_ARRVAL_P(opts), "headers", sizeof("headers"));
}
/* }}} */

/* {{{ proto bool HttpRequest::addCookies(array cookies)
 *
 * Add cookies.
 */
PHP_METHOD(HttpRequest, addCookies)
{
	zval *opts, **cookies, *new_cookies;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &new_cookies)) {
		RETURN_FALSE;
	}

	opts = GET_PROP(obj, options);

	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), "cookies", sizeof("cookies"), (void **) &cookies)) {
		array_merge(new_cookies, *cookies);
	} else {
		zval_add_ref(&new_cookies);
		add_assoc_zval(opts, "cookies", new_cookies);
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HttpRequest::getCookies()
 *
 * Get previously set cookies.
 */
PHP_METHOD(HttpRequest, getCookies)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *opts, **cookies;
		getObject(http_request_object, obj);

		opts = GET_PROP(obj, options);

		array_init(return_value);

		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), "cookies", sizeof("cookies"), (void **) &cookies)) {
			array_copy(*cookies, return_value);
		}
	}
}
/* }}} */

/* {{{ proto void HttpRequest::unsetCookies()
 *
 */
PHP_METHOD(HttpRequest, unsetCookies)
{
	zval *opts;
	getObject(http_request_object, obj);

	NO_ARGS;

	opts = GET_PROP(obj, options);
	zend_hash_del(Z_ARRVAL_P(opts), "cookies", sizeof("cookies"));
}
/* }}} */

/* {{{ proto bool HttpRequest::setURL(string url)
 *
 * Set the request URL.
 */
PHP_METHOD(HttpRequest, setURL)
{
	char *URL = NULL;
	int URL_len;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &URL, &URL_len)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, url, URL);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpRequest::getUrl()
 *
 * Get the previously set request URL.
 */
PHP_METHOD(HttpRequest, getURL)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *URL;
		getObject(http_request_object, obj);

		URL = GET_PROP(obj, url);
		RETURN_STRINGL(Z_STRVAL_P(URL), Z_STRLEN_P(URL), 1);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::setMethod(long request_method)
 *
 * Set the request methods; one of the <tt>HTTP_HEAD</tt>, <tt>HTTP_GET</tt> or
 * <tt>HTTP_POST</tt> constants.
 */
PHP_METHOD(HttpRequest, setMethod)
{
	long meth;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &meth)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, method, meth);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto long HttpRequest::getMethod()
 *
 * Get the previously set request method.
 */
PHP_METHOD(HttpRequest, getMethod)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *meth;
		getObject(http_request_object, obj);

		meth = GET_PROP(obj, method);
		RETURN_LONG(Z_LVAL_P(meth));
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::setContentType(string content_type)
 *
 * Set the content type the post request should have.
 * Use this only if you know what you're doing.
 */
PHP_METHOD(HttpRequest, setContentType)
{
	char *ctype;
	int ct_len;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ct_len)) {
		RETURN_FALSE;
	}

	if (!strchr(ctype, '/')) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Content-Type '%s' doesn't seem to contain a primary and a secondary part", ctype);
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, contentType, ctype);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpRequest::getContentType()
 *
 * Get the previously content type.
 */
PHP_METHOD(HttpRequest, getContentType)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *ctype;
		getObject(http_request_object, obj);

		ctype = GET_PROP(obj, contentType);
		RETURN_STRINGL(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::setQueryData(mixed query_data)
 *
 * Set the URL query parameters to use.
 * Overwrites previously set query parameters.
 * Affects any request types.
 */
PHP_METHOD(HttpRequest, setQueryData)
{
	zval *qdata;
	char *query_data = NULL;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &qdata)) {
		RETURN_FALSE;
	}

	if ((Z_TYPE_P(qdata) == IS_ARRAY) || (Z_TYPE_P(qdata) == IS_OBJECT)) {
		if (SUCCESS != http_urlencode_hash(HASH_OF(qdata), &query_data)) {
			RETURN_FALSE;
		}
		UPD_PROP(obj, string, queryData, query_data);
		efree(query_data);
		RETURN_TRUE;
	}

	convert_to_string(qdata);
	UPD_PROP(obj, string, queryData, Z_STRVAL_P(qdata));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpRequest::getQueryData()
 *
 * Get the current query data in form of an urlencoded query string.
 */
PHP_METHOD(HttpRequest, getQueryData)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *qdata;
		getObject(http_request_object, obj);

		qdata = GET_PROP(obj, queryData);
		RETURN_STRINGL(Z_STRVAL_P(qdata), Z_STRLEN_P(qdata), 1);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::addQueryData(array query_params)
 *
 * Add parameters to the query parameter list.
 * Affects any request type.
 */
PHP_METHOD(HttpRequest, addQueryData)
{
	zval *qdata, *old_qdata;
	char *query_data = NULL;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &qdata)) {
		RETURN_FALSE;
	}

	old_qdata = GET_PROP(obj, queryData);

	if (SUCCESS != http_urlencode_hash_ex(HASH_OF(qdata), 1, Z_STRVAL_P(old_qdata), Z_STRLEN_P(old_qdata), &query_data, NULL)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, queryData, query_data);
	efree(query_data);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto void HttpRequest::unsetQueryData()
 *
 * Clean the query parameters.
 * Affects any request type.
 */
PHP_METHOD(HttpRequest, unsetQueryData)
{
	getObject(http_request_object, obj);

	NO_ARGS;

	UPD_PROP(obj, string, queryData, "");
}
/* }}} */

/* {{{ proto bool HttpRequest::addPostFields(array post_data)
 *
 * Adds POST data entries.
 * Affects only POST requests.
 */
PHP_METHOD(HttpRequest, addPostFields)
{
	zval *post, *post_data;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &post_data)) {
		RETURN_FALSE;
	}

	post = GET_PROP(obj, postFields);
	array_merge(post_data, post);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HttpRequest::setPostFields(array post_data)
 *
 * Set the POST data entries.
 * Overwrites previously set POST data.
 * Affects only POST requests.
 */
PHP_METHOD(HttpRequest, setPostFields)
{
	zval *post, *post_data;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &post_data)) {
		RETURN_FALSE;
	}

	post = GET_PROP(obj, postFields);
	zend_hash_clean(Z_ARRVAL_P(post));
	array_copy(post_data, post);

	RETURN_TRUE;
}
/* }}}*/

/* {{{ proto array HttpRequest::getPostFields()
 *
 * Get previously set POST data.
 */
PHP_METHOD(HttpRequest, getPostFields)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *post_data;
		getObject(http_request_object, obj);

		post_data = GET_PROP(obj, postFields);
		array_init(return_value);
		array_copy(post_data, return_value);
	}
}
/* }}} */

/* {{{ proto void HttpRequest::unsetPostFields()
 *
 * Clean POST data entires.
 * Affects only POST requests.
 */
PHP_METHOD(HttpRequest, unsetPostFields)
{
	zval *post_data;
	getObject(http_request_object, obj);

	NO_ARGS;

	post_data = GET_PROP(obj, postFields);
	zend_hash_clean(Z_ARRVAL_P(post_data));
}
/* }}} */

/* {{{ proto bool HttpRequest::addPostFile(string name, string file[, string content_type = "application/x-octetstream"])
 *
 * Add a file to the POST request.
 * Affects only POST requests.
 */
PHP_METHOD(HttpRequest, addPostFile)
{
	zval *files, *entry;
	char *name, *file, *type = NULL;
	int name_len, file_len, type_len = 0;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|s", &name, &name_len, &file, &file_len, &type, &type_len)) {
		RETURN_FALSE;
	}

	if (type_len) {
		if (!strchr(type, '/')) {
			http_error_ex(E_WARNING, HTTP_E_PARAM, "Content-Type '%s' doesn't seem to contain a primary and a secondary part", type);
			RETURN_FALSE;
		}
	} else {
		type = "application/x-octetstream";
		type_len = sizeof("application/x-octetstream") - 1;
	}

	MAKE_STD_ZVAL(entry);
	array_init(entry);

	add_assoc_stringl(entry, "name", name, name_len, 1);
	add_assoc_stringl(entry, "type", type, type_len, 1);
	add_assoc_stringl(entry, "file", file, file_len, 1);

	files  = GET_PROP(obj, postFiles);
	add_next_index_zval(files, entry);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HttpRequest::setPostFiles()
 *
 * Set files to post.
 * Overwrites previously set post files.
 * Affects only POST requests.
 */
PHP_METHOD(HttpRequest, setPostFiles)
{
	zval *files, *pFiles;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &files)) {
		RETURN_FALSE;
	}

	pFiles = GET_PROP(obj, postFiles);
	zend_hash_clean(Z_ARRVAL_P(pFiles));
	array_copy(files, pFiles);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HttpRequest::getPostFiles()
 *
 * Get all previously added POST files.
 */
PHP_METHOD(HttpRequest, getPostFiles)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *files;
		getObject(http_request_object, obj);

		files = GET_PROP(obj, postFiles);

		array_init(return_value);
		array_copy(files, return_value);
	}
}
/* }}} */

/* {{{ proto void HttpRequest::unsetPostFiles()
 *
 * Unset the POST files list.
 * Affects only POST requests.
 */
PHP_METHOD(HttpRequest, unsetPostFiles)
{
	zval *files;
	getObject(http_request_object, obj);

	NO_ARGS;

	files = GET_PROP(obj, postFiles);
	zend_hash_clean(Z_ARRVAL_P(files));
}
/* }}} */

/* {{{ proto bool HttpRequest::SetPutFile(string file)
 *
 * Set file to put.
 * Affects only PUT requests.
 */
PHP_METHOD(HttpRequest, setPutFile)
{
	char *file;
	int file_len;
	getObject(http_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &file, &file_len)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, putFile, file);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpRequest::getPutFile()
 *
 * Get previously set put file.
 */
PHP_METHOD(HttpRequest, getPutFile)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *putfile;
		getObject(http_request_object, obj);

		putfile = GET_PROP(obj, putFile);
		RETVAL_STRINGL(Z_STRVAL_P(putfile), Z_STRLEN_P(putfile), 1);
	}
}
/* }}} */

/* {{{ proto void HttpRequest::unsetPutFile()
 *
 * Unset file to put.
 * Affects only PUT requests.
 */
PHP_METHOD(HttpRequest, unsetPutFile)
{
	getObject(http_request_object, obj);

	NO_ARGS;

	UPD_PROP(obj, string, putFile, "");
}
/* }}} */

/* {{{ proto array HttpRequest::getResponseData()
 *
 * Get all response data after the request has been sent.
 */
PHP_METHOD(HttpRequest, getResponseData)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *data;
		getObject(http_request_object, obj);

		data = GET_PROP(obj, responseData);
		array_init(return_value);
		array_copy(data, return_value);
	}
}
/* }}} */

/* {{{ proto mixed HttpRequest::getResponseHeader([string name])
 *
 * Get response header(s) after the request has been sent.
 */
PHP_METHOD(HttpRequest, getResponseHeader)
{
	IF_RETVAL_USED {
		zval *data, **headers, **header;
		char *header_name = NULL;
		int header_len = 0;
		getObject(http_response_object, obj);

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &header_name, &header_len)) {
			RETURN_FALSE;
		}

		data = GET_PROP(obj, responseData);
		if (SUCCESS != zend_hash_find(Z_ARRVAL_P(data), "headers", sizeof("headers"), (void **) &headers)) {
			RETURN_FALSE;
		}

		if (!header_len || !header_name) {
			array_init(return_value);
			array_copy(*headers, return_value);
		} else if (SUCCESS == zend_hash_find(Z_ARRVAL_PP(headers), pretty_key(header_name, header_len, 1, 1), header_len + 1, (void **) &header)) {
			RETURN_STRINGL(Z_STRVAL_PP(header), Z_STRLEN_PP(header), 1);
		} else {
			RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto array HttpRequest::getResponseCookie([string name])
 *
 * Get response cookie(s) after the request has been sent.
 */
PHP_METHOD(HttpRequest, getResponseCookie)
{
	IF_RETVAL_USED {
		zval *data, **headers;
		char *cookie_name = NULL;
		int cookie_len = 0;
		getObject(http_request_object, obj);

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &cookie_name, &cookie_len)) {
			RETURN_FALSE;
		}

		array_init(return_value);

		data = GET_PROP(obj, responseData);
		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(data), "headers", sizeof("headers"), (void **) &headers)) {
			ulong idx = 0;
			char *key = NULL;
			zval **header = NULL;

			FOREACH_HASH_KEYVAL(Z_ARRVAL_PP(headers), key, idx, header) {
				if (key && !strcasecmp(key, "Set-Cookie")) {
					/* several cookies? */
					if (Z_TYPE_PP(header) == IS_ARRAY) {
						zval **cookie;

						FOREACH_HASH_VAL(Z_ARRVAL_PP(header), cookie) {
							zval *cookie_hash;
							MAKE_STD_ZVAL(cookie_hash);
							array_init(cookie_hash);

							if (SUCCESS == http_parse_cookie(Z_STRVAL_PP(cookie), Z_ARRVAL_P(cookie_hash))) {
								if (!cookie_len) {
									add_next_index_zval(return_value, cookie_hash);
								} else {
									zval **name;

									if (	(SUCCESS == zend_hash_find(Z_ARRVAL_P(cookie_hash), "name", sizeof("name"), (void **) &name)) &&
											(!strcmp(Z_STRVAL_PP(name), cookie_name))) {
										add_next_index_zval(return_value, cookie_hash);
										return; /* <<< FOUND >>> */
									} else {
										zval_dtor(cookie_hash);
										efree(cookie_hash);
									}
								}
							} else {
								zval_dtor(cookie_hash);
								efree(cookie_hash);
							}
						}
					} else {
						zval *cookie_hash;
						MAKE_STD_ZVAL(cookie_hash);
						array_init(cookie_hash);

						if (SUCCESS == http_parse_cookie(Z_STRVAL_PP(header), Z_ARRVAL_P(cookie_hash))) {
							if (!cookie_len) {
								add_next_index_zval(return_value, cookie_hash);
							} else {
								zval **name;

								if (	(SUCCESS == zend_hash_find(Z_ARRVAL_P(cookie_hash), "name", sizeof("name"), (void **) &name)) &&
										(!strcmp(Z_STRVAL_PP(name), cookie_name))) {
									add_next_index_zval(return_value, cookie_hash);
								} else {
									zval_dtor(cookie_hash);
									efree(cookie_hash);
								}
							}
						} else {
							zval_dtor(cookie_hash);
							efree(cookie_hash);
						}
					}
					break;
				}
				/* reset key */
				key = NULL;
			}
		}
	}
}
/* }}} */

/* {{{ proto string HttpRequest::getResponseBody()
 *
 * Get the response body after the request has been sent.
 */
PHP_METHOD(HttpRequest, getResponseBody)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *data, **body;
		getObject(http_request_object, obj);

		data = GET_PROP(obj, responseData);
		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(data), "body", sizeof("body"), (void **) &body)) {
			RETURN_STRINGL(Z_STRVAL_PP(body), Z_STRLEN_PP(body), 1);
		} else {
			RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto int HttpRequest::getResponseCode()
 *
 * Get the response code after the request has been sent.
 */
PHP_METHOD(HttpRequest, getResponseCode)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *code;
		getObject(http_request_object, obj);

		code = GET_PROP(obj, responseCode);
		RETURN_LONG(Z_LVAL_P(code));
	}
}
/* }}} */

/* {{{ proto array HttpRequest::getResponseInfo([string name])
 *
 * Get response info after the request has been sent.
 * See http_get() for a full list of returned info.
 */
PHP_METHOD(HttpRequest, getResponseInfo)
{
	IF_RETVAL_USED {
		zval *info, **infop;
		char *info_name = NULL;
		int info_len = 0;
		getObject(http_request_object, obj);

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &info_name, &info_len)) {
			RETURN_FALSE;
		}

		info = GET_PROP(obj, responseInfo);

		if (info_len && info_name) {
			if (SUCCESS == zend_hash_find(Z_ARRVAL_P(info), pretty_key(info_name, info_len, 0, 0), info_len + 1, (void **) &infop)) {
				RETURN_ZVAL(*infop, 1, ZVAL_PTR_DTOR);
			} else {
				http_error_ex(E_NOTICE, HTTP_E_PARAM, "Could not find response info named %s", info_name);
				RETURN_FALSE;
			}
		} else {
			array_init(return_value);
			array_copy(info, return_value);
		}
	}
}
/* }}}*/

/* {{{ proto HttpMessage HttpRequest::getResponseMessage()
 *
 * Get the full response as HttpMessage object.
 */
PHP_METHOD(HttpRequest, getResponseMessage)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *message;
		getObject(http_request_object, obj);

		message = GET_PROP(obj, responseMessage);
		if (Z_TYPE_P(message) == IS_OBJECT) {
			RETVAL_OBJECT(message);
		} else {
			RETURN_NULL();
		}
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::send()
 *
 * Send the HTTP request.
 *
 * GET example:
 * <pre>
 * <?php
 * $r = new HttpRequest('http://example.com/feed.rss', HTTP_GET);
 * $r->setOptions(array('lastmodified' => filemtime('local.rss')));
 * $r->addQueryData(array('category' => 3));
 * try {
 *     $r->send();
 *     if ($r->getResponseCode() == 200) {
 *         file_put_contents('local.rss', $r->getResponseBody());
 *    }
 * } catch (HttpException $ex) {
 *     echo $ex;
 * }
 * ?>
 * </pre>
 *
 * POST example:
 * <pre>
 * <?php
 * $r = new HttpRequest('http://example.com/form.php', HTTP_POST);
 * $r->setOptions(array('cookies' => array('lang' => 'de')));
 * $r->addpostFields(array('user' => 'mike', 'pass' => 's3c|r3t'));
 * $r->addPostFile('image', 'profile.jpg', 'image/jpeg');
 * if ($r->send()) {
 *     echo $r->getResponseBody();
 * }
 * ?>
 * </pre>
 */
PHP_METHOD(HttpRequest, send)
{
	STATUS status = FAILURE;
	http_request_body body = {0};
	getObject(http_request_object, obj);

	NO_ARGS;

	SET_EH_THROW_HTTP();

	if (obj->pool) {
		http_error(E_WARNING, HTTP_E_CURL, "You cannot call HttpRequest::send() while attached to an HttpRequestPool");
		RETURN_FALSE;
	}

	if (SUCCESS == (status = http_request_object_requesthandler(obj, getThis(), &body))) {
		status = http_request_exec(obj->ch, NULL);
	}
	http_request_body_dtor(&body);

	/* final data handling */
	if (SUCCESS == status) {
		status = http_request_object_responsehandler(obj, getThis());
	}

	SET_EH_NORMAL();
	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ HttpRequestPool */

/* {{{ proto void HttpRequestPool::__construct([HttpRequest request[, ...]])
 *
 * Instantiate a new HttpRequestPool object.  An HttpRequestPool is
 * able to send several HttpRequests in parallel.
 *
 * Example:
 * <pre>
 * <?php
 *     $urls = array('www.php.net', 'pecl.php.net', 'pear.php.net')
 *     $pool = new HttpRequestPool;
 *     foreach ($urls as $url) {
 *         $req[$url] = new HttpRequest("http://$url", HTTP_HEAD);
 *         $pool->attach($req[$url]);
 *     }
 *     $pool->send();
 *     foreach ($urls as $url) {
 *         printf("%s (%s) is %s\n",
 *             $url, $req[$url]->getResponseInfo('effective_url'),
 *             $r->getResponseCode() == 200 ? 'alive' : 'not alive'
 *         );
 *     }
 * ?>
 * </pre>
 */
PHP_METHOD(HttpRequestPool, __construct)
{
	int argc = ZEND_NUM_ARGS();
	zval ***argv = safe_emalloc(argc, sizeof(zval *), 0);
	getObject(http_requestpool_object, obj);

	if (SUCCESS == zend_get_parameters_array_ex(argc, argv)) {
		int i;

		for (i = 0; i < argc; ++i) {
			if (Z_TYPE_PP(argv[i]) == IS_OBJECT && instanceof_function(Z_OBJCE_PP(argv[i]), http_request_object_ce TSRMLS_CC)) {
				http_request_pool_attach(&obj->pool, *(argv[i]));
			}
		}
	}
	efree(argv);
}
/* }}} */

/* {{{ proto void HttpRequestPool::__destruct()
 *
 * Clean up HttpRequestPool object.
 */
PHP_METHOD(HttpRequestPool, __destruct)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	http_request_pool_detach_all(&obj->pool);
}
/* }}} */

/* {{{ proto void HttpRequestPool::reset()
 *
 * Detach all attached HttpRequest objects.
 */
PHP_METHOD(HttpRequestPool, reset)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	http_request_pool_detach_all(&obj->pool);
}

/* {{{ proto bool HttpRequestPool::attach(HttpRequest request)
 *
 * Attach an HttpRequest object to this HttpRequestPool.
 * NOTE: set all options prior attaching!
 */
PHP_METHOD(HttpRequestPool, attach)
{
	zval *request;
	STATUS status = FAILURE;
	getObject(http_requestpool_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, http_request_object_ce)) {
		status = http_request_pool_attach(&obj->pool, request);
	}
	SET_EH_NORMAL();
	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ proto bool HttpRequestPool::detach(HttpRequest request)
 *
 * Detach an HttpRequest object from this HttpRequestPool.
 */
PHP_METHOD(HttpRequestPool, detach)
{
	zval *request;
	STATUS status = FAILURE;
	getObject(http_requestpool_object, obj);

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &request, http_request_object_ce)) {
		status = http_request_pool_detach(&obj->pool, request);
	}
	SET_EH_NORMAL();
	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ proto bool HttpRequestPool::send()
 *
 * Send all attached HttpRequest objects in parallel.
 */
PHP_METHOD(HttpRequestPool, send)
{
	STATUS status;
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	SET_EH_THROW_HTTP();
	status = http_request_pool_send(&obj->pool);
	SET_EH_NORMAL();

	RETURN_SUCCESS(status);
}
/* }}} */

/* {{{ proto protected bool HttpRequestPool::socketSend()
 *
 * Usage:
 * <pre>
 * <?php
 *     while ($pool->socketSend()) {
 *         do_something_else();
 *         if (!$pool->socketSelect()) {
 *             die('Socket error');
 *         }
 *     }
 *     $pool->socketRead();
 * ?>
 * </pre>
 */
PHP_METHOD(HttpRequestPool, socketSend)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	RETURN_BOOL(0 < http_request_pool_perform(&obj->pool));
}
/* }}} */

/* {{{ proto protected bool HttpRequestPool::socketSelect()
 *
 * See HttpRequestPool::socketSend().
 */
PHP_METHOD(HttpRequestPool, socketSelect)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	RETURN_SUCCESS(http_request_pool_select(&obj->pool));
}
/* }}} */

/* {{{ proto protected void HttpRequestPool::socketRead()
 *
 * See HttpRequestPool::socketSend().
 */
PHP_METHOD(HttpRequestPool, socketRead)
{
	getObject(http_requestpool_object, obj);

	NO_ARGS;

	zend_llist_apply(&obj->pool.handles, (llist_apply_func_t) http_request_pool_responsehandler TSRMLS_CC);
}
/* }}} */

/* }}} */

/* }}} */
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

