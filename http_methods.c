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
#include "php_http.h"
#include "php_http_api.h"
#include "php_http_curl_api.h"
#include "php_http_std_defs.h"

#include "ext/standard/php_smart_str.h"

#ifdef ZEND_ENGINE_2

/* {{{ HTTPi_Response */

/* {{{ proto void HTTPi_Response::__construct(bool cache, bool gzip)
 *
 * Instantiates a new HTTPi_Response object, which can be used to send
 * any data/resource/file to an HTTP client with caching and multiple
 * ranges/resuming support.
 *
 * NOTE: GZIPping is not implemented yet.
 */
PHP_METHOD(HTTPi_Response, __construct)
{
	zend_bool do_cache = 0, do_gzip = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|bb", &do_cache, &do_gzip)) {
		// throw exception
		return;
	}

	UPD_PROP(obj, long, cache, do_cache);
	UPD_PROP(obj, long, gzip, do_gzip);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setCache(bool cache)
 *
 * Whether it sould be attempted to cache the entitity.
 * This will result in necessary caching headers and checks of clients
 * "If-Modified-Since" and "If-None-Match" headers.  If one of those headers
 * matches a "304 Not Modified" status code will be issued.
 *
 * NOTE: If you're using sessions, be shure that you set session.cache_limiter
 * to something more appropriate than "no-cache"!
 */
PHP_METHOD(HTTPi_Response, setCache)
{
	zend_bool do_cache = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &do_cache)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, cache, do_cache);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HTTPi_Response::getCache()
 *
 * Get current caching setting.
 */
PHP_METHOD(HTTPi_Response, getCache)
{
	zval *do_cache = NULL;
	getObject(httpi_response_object, obj);

	NO_ARGS;

	do_cache = GET_PROP(obj, cache);
	RETURN_BOOL(Z_LVAL_P(do_cache));
}
/* }}}*/

/* {{{ proto bool HTTPi_Response::setGzip(bool gzip)
 *
 * Enable on-thy-fly gzipping of the sent entity. NOT IMPLEMENTED YET.
 */
PHP_METHOD(HTTPi_Response, setGzip)
{
	zend_bool do_gzip = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &do_gzip)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, gzip, do_gzip);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HTTPi_Response::getGzip()
 *
 * Get current gzipping setting.
 */
PHP_METHOD(HTTPi_Response, getGzip)
{
	zval *do_gzip = NULL;
	getObject(httpi_response_object, obj);

	NO_ARGS;

	do_gzip = GET_PROP(obj, gzip);
	RETURN_BOOL(Z_LVAL_P(do_gzip));
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setCacheControl(string control[, bool raw = false])
 *
 * Set a custom cache-control header, usually being "private" or "public";  if
 * $raw is set to true the header will be sent as-is.
 */
PHP_METHOD(HTTPi_Response, setCacheControl)
{
	char *ccontrol;
	int cc_len;
	zend_bool raw = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &ccontrol, &cc_len, &raw)) {
		RETURN_FALSE;
	}

	if ((!raw) && (strcmp(ccontrol, "public") && strcmp(ccontrol, "private") && strcmp(ccontrol, "no-cache"))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Cache-Control '%s' doesn't match public, private or no-cache", ccontrol);
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, raw_cache_header, raw);
	UPD_PROP(obj, string, cacheControl, ccontrol);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getCacheControl()
 *
 * Get current Cache-Control header setting.
 */
PHP_METHOD(HTTPi_Response, getCacheControl)
{
	zval *ccontrol;
	getObject(httpi_response_object, obj);

	NO_ARGS;

	ccontrol = GET_PROP(obj, cacheControl);
	RETURN_STRINGL(Z_STRVAL_P(ccontrol), Z_STRLEN_P(ccontrol), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setContentType(string content_type)
 *
 * Set the content-type of the sent entity.
 */
PHP_METHOD(HTTPi_Response, setContentType)
{
	char *ctype;
	int ctype_len;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ctype_len)) {
		RETURN_FALSE;
	}

	if (!strchr(ctype, '/')) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Content type '%s' doesn't seem to contain a primary and a secondary part", ctype);
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, contentType, ctype);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getContentType()
 *
 * Get current Content-Type header setting.
 */
PHP_METHOD(HTTPi_Response, getContentType)
{
	zval *ctype;
	getObject(httpi_response_object, obj);

	NO_ARGS;

	ctype = GET_PROP(obj, contentType);
	RETURN_STRINGL(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setContentDisposition(string filename[, bool inline = false])
 *
 * Set the Content-Disposition of the sent entity.  This setting aims to suggest
 * the receiveing user agent how to handle the sent entity;  usually the client
 * will show the user a "Save As..." popup.
 */
PHP_METHOD(HTTPi_Response, setContentDisposition)
{
	char *file;
	int file_len;
	zend_bool is_inline = 0;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &file, &file_len, &is_inline)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, dispoFile, file);
	UPD_PROP(obj, long, dispoInline, is_inline);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HTTPi_Response::getContentDisposition()
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
PHP_METHOD(HTTPi_Response, getContentDisposition)
{
	zval *file;
	zval *is_inline;
	getObject(httpi_response_object, obj);

	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	file = GET_PROP(obj, dispoFile);
	is_inline = GET_PROP(obj, dispoInline);

	array_init(return_value);
	add_assoc_stringl(return_value, "filename", Z_STRVAL_P(file), Z_STRLEN_P(file), 1);
	add_assoc_bool(return_value, "inline", Z_LVAL_P(is_inline));
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setETag(string etag)
 *
 * Set a custom ETag.  Use this only if you know what you're doing.
 */
PHP_METHOD(HTTPi_Response, setETag)
{
	char *etag;
	int etag_len;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &etag, &etag_len)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, eTag, etag);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getETag()
 *
 * Get the previously set custom ETag.
 */
PHP_METHOD(HTTPi_Response, getETag)
{
	zval *etag;
	getObject(httpi_response_object, obj);

	NO_ARGS;

	etag = GET_PROP(obj, eTag);
	RETURN_STRINGL(Z_STRVAL_P(etag), Z_STRLEN_P(etag), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setData(string data)
 *
 * Set the data to be sent.
 */
PHP_METHOD(HTTPi_Response, setData)
{
	zval *the_data;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &the_data)) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&the_data);
	SET_PROP(obj, data, the_data);
	UPD_PROP(obj, long, lastModified, http_lmod(the_data, SEND_DATA));
	UPD_PROP(obj, long, send_mode, SEND_DATA);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getData()
 *
 * Get the previously set data to be sent.
 */
PHP_METHOD(HTTPi_Response, getData)
{
	zval *the_data;
	getObject(httpi_response_object, obj);

	NO_ARGS;

	the_data = GET_PROP(obj, data);
	RETURN_STRINGL(Z_STRVAL_P(the_data), Z_STRLEN_P(the_data), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setStream(resource stream)
 *
 * Set the resource to be sent.
 */
PHP_METHOD(HTTPi_Response, setStream)
{
	zval *the_stream;
	php_stream *the_real_stream;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &the_stream)) {
		RETURN_FALSE;
	}

	php_stream_from_zval(the_real_stream, &the_stream);

	SET_PROP(obj, stream, the_stream);
	UPD_PROP(obj, long, lastModified, http_lmod(the_real_stream, SEND_RSRC));
	UPD_PROP(obj, long, send_mode, SEND_RSRC);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto resource HTTPi_Response::getStream()
 *
 * Get the previously set resource to be sent.
 */
PHP_METHOD(HTTPi_Response, getStream)
{
	zval *the_stream;
	getObject(httpi_response_object, obj);

	NO_ARGS;

	the_stream = GET_PROP(obj, stream);
	RETURN_RESOURCE(Z_LVAL_P(the_stream));
}
/* }}} */

/* {{{ proto bool HTTPi_Response::setFile(string file)
 *
 * Set the file to be sent.
 */
PHP_METHOD(HTTPi_Response, setFile)
{
	zval *the_file;
	getObject(httpi_response_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &the_file)) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&the_file);

	UPD_PROP(obj, string, file, Z_STRVAL_P(the_file));
	UPD_PROP(obj, long, lastModified, http_lmod(the_file, -1));
	UPD_PROP(obj, long, send_mode, -1);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Response::getFile()
 *
 * Get the previously set file to be sent.
 */
PHP_METHOD(HTTPi_Response, getFile)
{
	zval *the_file;
	getObject(httpi_response_object, obj);

	NO_ARGS;

	the_file = GET_PROP(obj, file);
	RETURN_STRINGL(Z_STRVAL_P(the_file), Z_STRLEN_P(the_file), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Response::send()
 *
 * Finally send the entity.
 *
 * Example:
 * <pre>
 * <?php
 * $r = new HTTPi_Response(true);
 * $r->setFile('../hidden/contract.pdf');
 * $r->setContentType('application/pdf');
 * $r->send();
 * ?>
 * </pre>
 *
 */
PHP_METHOD(HTTPi_Response, send)
{
	zval *do_cache, *do_gzip;
	getObject(httpi_response_object, obj);

	NO_ARGS;

	do_cache = GET_PROP(obj, cache);
	do_gzip  = GET_PROP(obj, gzip);

	/* caching */
	if (Z_LVAL_P(do_cache)) {
		zval *cctrl, *etag, *lmod, *ccraw;

		etag  = GET_PROP(obj, eTag);
		lmod  = GET_PROP(obj, lastModified);
		cctrl = GET_PROP(obj, cacheControl);
		ccraw = GET_PROP(obj, raw_cache_header);

		if (Z_LVAL_P(ccraw)) {
			http_cache_etag(Z_STRVAL_P(etag), Z_STRLEN_P(etag), Z_STRVAL_P(cctrl), Z_STRLEN_P(cctrl));
			http_cache_last_modified(Z_LVAL_P(lmod), Z_LVAL_P(lmod) ? Z_LVAL_P(lmod) : time(NULL), Z_STRVAL_P(cctrl), Z_STRLEN_P(cctrl));
		} else {
			char cc_header[42] = {0};
			sprintf(cc_header, "%s, must-revalidate, max-age=0", Z_STRVAL_P(cctrl));
			http_cache_etag(Z_STRVAL_P(etag), Z_STRLEN_P(etag), cc_header, strlen(cc_header));
			http_cache_last_modified(Z_LVAL_P(lmod), Z_LVAL_P(lmod) ? Z_LVAL_P(lmod) : time(NULL), cc_header, strlen(cc_header));
		}
	}

	/* gzip */
	if (Z_LVAL_P(do_gzip)) {
		/* ... */
	}

	/* content type */
	{
		zval *ctype = GET_PROP(obj, contentType);
		if (Z_STRLEN_P(ctype)) {
			http_send_content_type(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype));
		} else {
			http_send_content_type("application/x-octetstream", sizeof("application/x-octetstream") - 1);
		}
	}

	/* content disposition */
	{
		zval *dispo_file = GET_PROP(obj, dispoFile);
		if (Z_STRLEN_P(dispo_file)) {
			zval *dispo_inline = GET_PROP(obj, dispoInline);
			http_send_content_disposition(Z_STRVAL_P(dispo_file), Z_STRLEN_P(dispo_file), Z_LVAL_P(dispo_inline));
		}
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

#ifdef HTTP_HAVE_CURL
/* {{{ HTTPi_Request */

/* {{{ proto void HTTPi_Request::__construct([string url[, long request_method = HTTP_GET]])
 *
 * Instantiate a new HTTPi_Request object which can be used to issue HEAD, GET
 * and POST (including posting files) HTTP requests.
 */
PHP_METHOD(HTTPi_Request, __construct)
{
	char *URL = NULL;
	int URL_len;
	long meth = -1;
	getObject(httpi_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sl", &URL, &URL_len, &meth)) {
		return;
	}

	INIT_PARR(obj, options);
	INIT_PARR(obj, responseInfo);
	INIT_PARR(obj, responseData);
	INIT_PARR(obj, postData);
	INIT_PARR(obj, postFiles);

	if (URL) {
		UPD_PROP(obj, string, url, URL);
	}
	if (meth > -1) {
		UPD_PROP(obj, long, method, meth);
	}
}
/* }}} */

/* {{{ proto void HTTPi_Request::__destruct()
 *
 * Destroys the HTTPi_Request object.
 */
PHP_METHOD(HTTPi_Request, __destruct)
{
	getObject(httpi_request_object, obj);

	NO_ARGS;

	FREE_PARR(obj, options);
	FREE_PARR(obj, responseInfo);
	FREE_PARR(obj, responseData);
	FREE_PARR(obj, postData);
	FREE_PARR(obj, postFiles);
}
/* }}} */

/* {{{ proto bool HTTPi_Request::setOptions(array options)
 *
 * Set the request options to use.  See http_get() for a full list of available options.
 */
PHP_METHOD(HTTPi_Request, setOptions)
{
	char *key = NULL;
	long idx = 0;
	zval *opts, *old_opts, **opt;
	getObject(httpi_request_object, obj);

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

/* {{{ proto array HTTPi_Request::getOptions()
 *
 * Get current set options.
 */
PHP_METHOD(HTTPi_Request, getOptions)
{
	zval *opts;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	opts = GET_PROP(obj, options);
	array_init(return_value);
	array_copy(opts, return_value);
}
/* }}} */

/* {{{ proto void HTTPi_Request::unsetOptions()
 *
 * Unset all options/headers/cookies.
 */
PHP_METHOD(HTTPi_Request, unsetOptions)
{
	getObject(httpi_request_object, obj);
	
	NO_ARGS;
	
	FREE_PARR(obj, options);
	INIT_PARR(obj, options);
}
/* }}} */

/* {{{ proto bool HTTPi_Request::addHeader(array header)
 *
 * Add (a) request header name/value pair(s).
 */
PHP_METHOD(HTTPi_Request, addHeader)
{
	zval *opts, **headers, *new_headers;
	getObject(httpi_request_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &new_headers)) {
		RETURN_FALSE;
	}
	
	opts = GET_PROP(obj, options);
	
	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), "headers", sizeof("headers"), (void **) &headers)) {
		array_merge(new_headers, *headers);
	} else {
		zval_add_ref(new_headers);
		add_assoc_zval(opts, "headers", new_headers);
	}
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HTTPi_Request::addCookie(array cookie)
 *
 * Add (a) cookie(s).
 */
PHP_METHOD(HTTPi_Request, addCookie)
{
	zval *opts, **cookies, *new_cookies;
	getObject(httpi_request_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &new_cookies)) {
		RETURN_FALSE;
	}
	
	opts = GET_PROP(obj, options);
	
	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), "cookies", sizeof("cookies"), (void **) &cookies)) {
		array_merge(new_cookies, *cookies);
	} else {
		zval_add_ref(new_cookies);
		add_assoc_zval(opts, "cookies", new_cookies);
	}
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HTTPi_Request::setURL(string url)
 *
 * Set the request URL.
 */
PHP_METHOD(HTTPi_Request, setURL)
{
	char *URL = NULL;
	int URL_len;
	getObject(httpi_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &URL, &URL_len)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, url, URL);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Request::getUrl()
 *
 * Get the previously set request URL.
 */
PHP_METHOD(HTTPi_Request, getURL)
{
	zval *URL;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	URL = GET_PROP(obj, url);
	RETURN_STRINGL(Z_STRVAL_P(URL), Z_STRLEN_P(URL), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Request::setMethod(long request_method)
 *
 * Set the request methods; one of the <tt>HTTP_HEAD</tt>, <tt>HTTP_GET</tt> or
 * <tt>HTTP_POST</tt> constants.
 */
PHP_METHOD(HTTPi_Request, setMethod)
{
	long meth;
	getObject(httpi_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &meth)) {
		RETURN_FALSE;
	}

	UPD_PROP(obj, long, method, meth);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto long HTTPi_Request::getMethod()
 *
 * Get the previously set request method.
 */
PHP_METHOD(HTTPi_Request, getMethod)
{
	zval *meth;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	meth = GET_PROP(obj, method);
	RETURN_LONG(Z_LVAL_P(meth));
}
/* }}} */

/* {{{ proto bool HTTPi_Request::setContentType(string content_type)
 *
 * Set the content type the post request should have.
 * Use this only if you know what you're doing.
 */
PHP_METHOD(HTTPi_Request, setContentType)
{
	char *ctype;
	int ct_len;
	getObject(httpi_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ct_len)) {
		RETURN_FALSE;
	}

	if (!strchr(ctype, '/')) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Content-Type '%s' doesn't seem to contain a primary and a secondary part",
			ctype);
		RETURN_FALSE;
	}

	UPD_PROP(obj, string, contentType, ctype);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HTTPi_Request::getContentType()
 *
 * Get the previously content type.
 */
PHP_METHOD(HTTPi_Request, getContentType)
{
	zval *ctype;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	ctype = GET_PROP(obj, contentType);
	RETURN_STRINGL(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Request::setQueryData(mixed query_data)
 *
 * Set the URL query parameters to use.
 * Overwrites previously set query parameters.
 * Affects any request types.
 */
PHP_METHOD(HTTPi_Request, setQueryData)
{
	zval *qdata;
	char *query_data = NULL;
	getObject(httpi_request_object, obj);

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

/* {{{ proto string HTTPi_Request::getQueryData()
 *
 * Get the current query data in form of an urlencoded query string.
 */
PHP_METHOD(HTTPi_Request, getQueryData)
{
	zval *qdata;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	qdata = GET_PROP(obj, queryData);
	RETURN_STRINGL(Z_STRVAL_P(qdata), Z_STRLEN_P(qdata), 1);
}
/* }}} */

/* {{{ proto bool HTTPi_Request::addQueryData(array query_params)
 *
 * Add parameters to the query parameter list.
 * Affects any request type.
 */
PHP_METHOD(HTTPi_Request, addQueryData)
{
	zval *qdata, *old_qdata;
	char *query_data = NULL;
	getObject(httpi_request_object, obj);

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

/* {{{ proto void HTTPi_Request::unsetQueryData()
 *
 * Clean the query parameters.
 * Affects any request type.
 */
PHP_METHOD(HTTPi_Request, unsetQueryData)
{
	getObject(httpi_request_object, obj);

	NO_ARGS;

	UPD_PROP(obj, string, queryData, "");
}
/* }}} */

/* {{{ proto bool HTTPi_Request::addPostData(array post_data)
 *
 * Adds POST data entries.
 * Affects only POST requests.
 */
PHP_METHOD(HTTPi_Request, addPostData)
{
	zval *post, *post_data;
	getObject(httpi_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &post_data)) {
		RETURN_FALSE;
	}

	post = GET_PROP(obj, postData);
	array_merge(post_data, post);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HTTPi_Request::setPostData(array post_data)
 *
 * Set the POST data entries.
 * Overwrites previously set POST data.
 * Affects only POST requests.
 */
PHP_METHOD(HTTPi_Request, setPostData)
{
	zval *post, *post_data;
	getObject(httpi_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &post_data)) {
		RETURN_FALSE;
	}

	post = GET_PROP(obj, postData);
	zend_hash_clean(Z_ARRVAL_P(post));
	array_copy(post_data, post);

	RETURN_TRUE;
}
/* }}}*/

/* {{{ proto array HTTPi_Request::getPostData()
 *
 * Get previously set POST data.
 */
PHP_METHOD(HTTPi_Request, getPostData)
{
	zval *post_data;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	post_data = GET_PROP(obj, postData);
	array_init(return_value);
	array_copy(post_data, return_value);
}
/* }}} */

/* {{{ proto void HTTPi_Request::unsetPostData()
 *
 * Clean POST data entires.
 * Affects only POST requests.
 */
PHP_METHOD(HTTPi_Request, unsetPostData)
{
	zval *post_data;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	post_data = GET_PROP(obj, postData);
	zend_hash_clean(Z_ARRVAL_P(post_data));
}
/* }}} */

/* {{{ proto bool HTTPi_Request::addPostFile(string name, string file[, string content_type = "application/x-octetstream"])
 *
 * Add a file to the POST request.
 * Affects only POST requests.
 */
PHP_METHOD(HTTPi_Request, addPostFile)
{
	zval *files, *entry;
	char *name, *file, *type = NULL;
	int name_len, file_len, type_len = 0;
	getObject(httpi_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|s", &name, &name_len, &file, &file_len, &type, &type_len)) {
		RETURN_FALSE;
	}

	if (type_len) {
		if (!strchr(type, '/')) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Content-Type '%s' doesn't seem to contain a primary and a secondary part", type);
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

/* {{{ proto array HTTPi_Request::getPostFiles()
 *
 * Get all previously added POST files.
 */
PHP_METHOD(HTTPi_Request, getPostFiles)
{
	zval *files;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	files = GET_PROP(obj, postFiles);

	array_init(return_value);
	array_copy(files, return_value);
}
/* }}} */

/* {{{ proto void HTTPi_Request::unsetPostFiles()
 *
 * Unset the POST files list.
 * Affects only POST requests.
 */
PHP_METHOD(HTTPi_Request, unsetPostFiles)
{
	zval *files;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	files = GET_PROP(obj, postFiles);
	zend_hash_clean(Z_ARRVAL_P(files));
}
/* }}} */

/* {{{ proto array HTTPi_Request::getResponseData()
 *
 * Get all response data after the request has been sent.
 */
PHP_METHOD(HTTPi_Request, getResponseData)
{
	zval *data;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	data = GET_PROP(obj, responseData);
	array_init(return_value);
	array_copy(data, return_value);
}
/* }}} */

/* {{{ proto string HTTPi_Request::getResponseHeader([string name])
 *
 * Get response header(s) after the request has been sent.
 */
PHP_METHOD(HTTPi_Request, getResponseHeader)
{
	zval *data, **headers, **header;
	char *header_name = NULL;
	int header_len = 0;
	getObject(httpi_response_object, obj);

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

/* {{{ proto string HTTPi_Request::getResponseBody()
 *
 * Get the response body after the request has been sent.
 */
PHP_METHOD(HTTPi_Request, getResponseBody)
{
	zval *data, **body;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	data = GET_PROP(obj, responseData);
	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(data), "body", sizeof("body"), (void **) &body)) {
		RETURN_STRINGL(Z_STRVAL_PP(body), Z_STRLEN_PP(body), 1);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto int HTTPi_Request::getResponseCode()
 *
 * Get the response code after the request has been sent.
 */
PHP_METHOD(HTTPi_Request, getResponseCode)
{
	zval **code, **hdrs, *data;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	data = GET_PROP(obj, responseData);
	if (	(SUCCESS == zend_hash_find(Z_ARRVAL_P(data), "headers", sizeof("headers"), (void **) &hdrs)) &&
			(SUCCESS == zend_hash_find(Z_ARRVAL_PP(hdrs), "Status", sizeof("Status"), (void **) &code))) {
		RETVAL_STRINGL(Z_STRVAL_PP(code), Z_STRLEN_PP(code), 1);
		convert_to_long(return_value);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array HTTPi_Request::getResponseInfo([string name])
 *
 * Get response info after the request has been sent.
 * See http_get() for a full list of returned info.
 */
PHP_METHOD(HTTPi_Request, getResponseInfo)
{
	zval *info, **infop;
	char *info_name = NULL;
	int info_len = 0;
	getObject(httpi_request_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &info_name, &info_len)) {
		RETURN_FALSE;
	}

	info = GET_PROP(obj, responseInfo);

	if (info_len && info_name) {
		if (SUCCESS == zend_hash_find(Z_ARRVAL_P(info), pretty_key(info_name, info_len, 0, 0), info_len + 1, (void **) &infop)) {
			RETURN_ZVAL(*infop, 1, ZVAL_PTR_DTOR);
		} else {
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Could not find response info named %s", info_name);
			RETURN_FALSE;
		}
	} else {
		array_init(return_value);
		array_copy(info, return_value);
	}
}
/* }}}*/

/* {{{ proto bool HTTPi_Request::send()
 *
 * Send the HTTP request.
 *
 * GET example:
 * <pre>
 * <?php
 * $r = new HTTPi_Request('http://example.com/feed.rss', HTTP_GET);
 * $r->setOptions(array('lastmodified' => filemtime('local.rss')));
 * $r->addQueryData(array('category' => 3));
 * if ($r->send() && $r->getResponseCode() == 200) {
 *     file_put_contents('local.rss', $r->getResponseBody());
 * }
 * ?>
 * </pre>
 *
 * POST example:
 * <pre>
 * <?php
 * $r = new HTTPi_Request('http://example.com/form.php', HTTP_POST);
 * $r->setOptions(array('cookies' => array('lang' => 'de')));
 * $r->addPostData(array('user' => 'mike', 'pass' => 's3c|r3t'));
 * $r->addPostFile('image', 'profile.jpg', 'image/jpeg');
 * if ($r->send()) {
 *     echo $r->getResponseBody();
 * }
 * ?>
 * </pre>
 */
PHP_METHOD(HTTPi_Request, send)
{
	STATUS status = FAILURE;
	zval *meth, *URL, *qdata, *opts, *info, *resp;
	char *response_data, *request_uri;
	size_t response_len;
	getObject(httpi_request_object, obj);

	NO_ARGS;

	if ((!obj->ch) && (!(obj->ch = curl_easy_init()))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not initilaize cURL");
		RETURN_FALSE;
	}

	meth  = GET_PROP(obj, method);
	URL   = GET_PROP(obj, url);
	qdata = GET_PROP(obj, queryData);
	opts  = GET_PROP(obj, options);
	info  = GET_PROP(obj, responseInfo);
	resp  = GET_PROP(obj, responseData);

	// HTTP_URI_MAXLEN+1 long char *
	request_uri = http_absolute_uri_ex(Z_STRVAL_P(URL), Z_STRLEN_P(URL), NULL, 0, NULL, 0, 0);

	if (Z_STRLEN_P(qdata) && (strlen(request_uri) < HTTP_URI_MAXLEN)) {
		if (!strchr(request_uri, '?')) {
			strcat(request_uri, "?");
		} else {
			strcat(request_uri, "&");
		}
		strncat(request_uri, Z_STRVAL_P(qdata), HTTP_URI_MAXLEN - strlen(request_uri));
	}

	switch (Z_LVAL_P(meth))
	{
		case HTTP_GET:
			status = http_get_ex(obj->ch, request_uri, Z_ARRVAL_P(opts), Z_ARRVAL_P(info), &response_data, &response_len);
		break;

		case HTTP_HEAD:
			status = http_head_ex(obj->ch, request_uri, Z_ARRVAL_P(opts), Z_ARRVAL_P(info), &response_data, &response_len);
		break;

		case HTTP_POST:
			{
				zval *post_files, *post_data;

				post_files = GET_PROP(obj, postFiles);
				post_data  = GET_PROP(obj, postData);

				if (!zend_hash_num_elements(Z_ARRVAL_P(post_files))) {

					/* urlencoded post */
					status = http_post_array_ex(obj->ch, request_uri, Z_ARRVAL_P(post_data), Z_ARRVAL_P(opts), Z_ARRVAL_P(info), &response_data, &response_len);

				} else {

					/*
					 * multipart post
					 */
					char *key = NULL;
					long idx;
					zval **data;
					struct curl_httppost *http_post_data[2] = {NULL, NULL};

					/* normal data */
					FOREACH_KEYVAL(post_data, key, idx, data) {
						if (key) {
							convert_to_string_ex(data);
							curl_formadd(&http_post_data[0], &http_post_data[1],
								CURLFORM_COPYNAME,			key,
								CURLFORM_COPYCONTENTS,		Z_STRVAL_PP(data),
								CURLFORM_CONTENTSLENGTH,	Z_STRLEN_PP(data),
								CURLFORM_END
							);

							/* reset */
							key = NULL;
						}
					}

					/* file data */
					FOREACH_VAL(post_files, data) {
						zval **file, **type, **name;

						if (	SUCCESS == zend_hash_find(Z_ARRVAL_PP(data), "name", sizeof("name"), (void **) &name) &&
								SUCCESS == zend_hash_find(Z_ARRVAL_PP(data), "type", sizeof("type"), (void **) &type) &&
								SUCCESS == zend_hash_find(Z_ARRVAL_PP(data), "file", sizeof("file"), (void **) &file)) {

							curl_formadd(&http_post_data[0], &http_post_data[1],
								CURLFORM_COPYNAME,		Z_STRVAL_PP(name),
								CURLFORM_FILE,			Z_STRVAL_PP(file),
								CURLFORM_CONTENTTYPE,	Z_STRVAL_PP(type),
								CURLFORM_END
							);
						}
					}

					status = http_post_curldata_ex(obj->ch, request_uri, http_post_data[0], Z_ARRVAL_P(opts), Z_ARRVAL_P(info), &response_data, &response_len);
					curl_formfree(http_post_data[0]);
				}
			}
		break;

		default:
		break;
	}

	efree(request_uri);

	/* final data handling */
	if (status != SUCCESS) {
		RETURN_FALSE;
	} else {
		char *body = NULL;
		size_t body_len = 0;
		zval *zheaders;

		MAKE_STD_ZVAL(zheaders)
		array_init(zheaders);

		if (SUCCESS != http_split_response_ex(response_data, response_len, Z_ARRVAL_P(zheaders), &body, &body_len)) {
			zval_dtor(zheaders);
			efree(zheaders),
			efree(response_data);
			RETURN_FALSE;
		}

		add_assoc_zval(resp, "headers", zheaders);
		add_assoc_stringl(resp, "body", body, body_len, 0);

		efree(response_data);

		RETURN_TRUE;
	}
	/* */
}
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

