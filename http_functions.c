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
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/session/php_session.h"
#include "ext/standard/php_string.h"

#include "SAPI.h"

#include "phpstr/phpstr.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_auth_api.h"
#include "php_http_request_api.h"
#include "php_http_cache_api.h"
#include "php_http_request_api.h"
#include "php_http_date_api.h"
#include "php_http_headers_api.h"
#include "php_http_message_api.h"
#include "php_http_send_api.h"
#include "php_http_url_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http)

/* {{{ proto string http_date([int timestamp])
 *
 * This function returns a valid HTTP date regarding RFC 822/1123
 * looking like: "Wed, 22 Dec 2004 11:34:47 GMT"
 *
 */
PHP_FUNCTION(http_date)
{
	long t = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &t) != SUCCESS) {
		RETURN_FALSE;
	}

	if (t == -1) {
		t = (long) time(NULL);
	}

	RETURN_STRING(http_date(t), 0);
}
/* }}} */

/* {{{ proto string http_absolute_uri(string url[, string proto[, string host[, int port]]])
 *
 * This function returns an absolute URI constructed from url.
 * If the url is already abolute but a different proto was supplied,
 * only the proto part of the URI will be updated.  If url has no
 * path specified, the path of the current REQUEST_URI will be taken.
 * The host will be taken either from the Host HTTP header of the client
 * the SERVER_NAME or just localhost if prior are not available.
 *
 * Some examples:
 * <pre>
 *  url = "page.php"                    => http://www.example.com/current/path/page.php
 *  url = "/page.php"                   => http://www.example.com/page.php
 *  url = "/page.php", proto = "https"  => https://www.example.com/page.php
 * </pre>
 *
 */
PHP_FUNCTION(http_absolute_uri)
{
	char *url = NULL, *proto = NULL, *host = NULL;
	int url_len = 0, proto_len = 0, host_len = 0;
	long port = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|ssl", &url, &url_len, &proto, &proto_len, &host, &host_len, &port) != SUCCESS) {
		RETURN_FALSE;
	}

	RETURN_STRING(http_absolute_uri_ex(url, url_len, proto, proto_len, host, host_len, port), 0);
}
/* }}} */

/* {{{ proto string http_negotiate_language(array supported[, string default = 'en-US'])
 *
 * This function negotiates the clients preferred language based on its
 * Accept-Language HTTP header.  It returns the negotiated language or
 * the default language if none match.
 *
 * The qualifier is recognized and languages without qualifier are rated highest.
 *
 * The supported parameter is expected to be an array having
 * the supported languages as array values.
 *
 * Example:
 * <pre>
 * <?php
 * $langs = array(
 * 		'en-US',// default
 * 		'fr',
 * 		'fr-FR',
 * 		'de',
 * 		'de-DE',
 * 		'de-AT',
 * 		'de-CH',
 * );
 * include './langs/'. http_negotiate_language($langs) .'.php';
 * ?>
 * </pre>
 *
 */
PHP_FUNCTION(http_negotiate_language)
{
	zval *supported;
	char *def = NULL;
	int def_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|s", &supported, &def, &def_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!def) {
		def = "en-US";
	}

	RETURN_STRING(http_negotiate_language(supported, def), 0);
}
/* }}} */

/* {{{ proto string http_negotiate_charset(array supported[, string default = 'iso-8859-1'])
 *
 * This function negotiates the clients preferred charset based on its
 * Accept-Charset HTTP header.  It returns the negotiated charset or
 * the default charset if none match.
 *
 * The qualifier is recognized and charset without qualifier are rated highest.
 *
 * The supported parameter is expected to be an array having
 * the supported charsets as array values.
 *
 * Example:
 * <pre>
 * <?php
 * $charsets = array(
 * 		'iso-8859-1', // default
 * 		'iso-8859-2',
 * 		'iso-8859-15',
 * 		'utf-8'
 * );
 * $pref = http_negotiate_charset($charsets);
 * if (!strcmp($pref, 'iso-8859-1')) {
 * 		iconv_set_encoding('internal_encoding', 'iso-8859-1');
 * 		iconv_set_encoding('output_encoding', $pref);
 * 		ob_start('ob_iconv_handler');
 * }
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_negotiate_charset)
{
	zval *supported;
	char *def = NULL;
	int def_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|s", &supported, &def, &def_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!def) {
		def = "iso-8859-1";
	}

	RETURN_STRING(http_negotiate_charset(supported, def), 0);
}
/* }}} */

/* {{{ proto bool http_send_status(int status)
 *
 * Send HTTP status code.
 *
 */
PHP_FUNCTION(http_send_status)
{
	int status = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &status) != SUCCESS) {
		RETURN_FALSE;
	}
	if (status < 100 || status > 510) {
		http_error_ex(E_WARNING, HTTP_E_HEADER, "Invalid HTTP status code (100-510): %d", status);
		RETURN_FALSE;
	}

	RETURN_SUCCESS(http_send_status(status));
}
/* }}} */

/* {{{ proto bool http_send_last_modified([int timestamp])
 *
 * This converts the given timestamp to a valid HTTP date and
 * sends it as "Last-Modified" HTTP header.  If timestamp is
 * omitted, current time is sent.
 *
 */
PHP_FUNCTION(http_send_last_modified)
{
	long t = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &t) != SUCCESS) {
		RETURN_FALSE;
	}

	if (t == -1) {
		t = (long) time(NULL);
	}

	RETURN_SUCCESS(http_send_last_modified(t));
}
/* }}} */

/* {{{ proto bool http_send_content_type([string content_type = 'application/x-octetstream'])
 *
 * Sets the content type.
 *
 */
PHP_FUNCTION(http_send_content_type)
{
	char *ct;
	int ct_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &ct, &ct_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!ct_len) {
		RETURN_SUCCESS(http_send_content_type("application/x-octetstream", lenof("application/x-octetstream")));
	}
	RETURN_SUCCESS(http_send_content_type(ct, ct_len));
}
/* }}} */

/* {{{ proto bool http_send_content_disposition(string filename[, bool inline = false])
 *
 * Set the Content Disposition.  The Content-Disposition header is very useful
 * if the data actually sent came from a file or something similar, that should
 * be "saved" by the client/user (i.e. by browsers "Save as..." popup window).
 *
 */
PHP_FUNCTION(http_send_content_disposition)
{
	char *filename;
	int f_len;
	zend_bool send_inline = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &filename, &f_len, &send_inline) != SUCCESS) {
		RETURN_FALSE;
	}
	RETURN_SUCCESS(http_send_content_disposition(filename, f_len, send_inline));
}
/* }}} */

/* {{{ proto bool http_match_modified([int timestamp[, for_range = false]])
 *
 * Matches the given timestamp against the clients "If-Modified-Since" resp.
 * "If-Unmodified-Since" HTTP headers.
 *
 */
PHP_FUNCTION(http_match_modified)
{
	long t = -1;
	zend_bool for_range = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lb", &t, &for_range) != SUCCESS) {
		RETURN_FALSE;
	}

	// current time if not supplied (senseless though)
	if (t == -1) {
		t = (long) time(NULL);
	}

	if (for_range) {
		RETURN_BOOL(http_match_last_modified("HTTP_IF_UNMODIFIED_SINCE", t));
	}
	RETURN_BOOL(http_match_last_modified("HTTP_IF_MODIFIED_SINCE", t));
}
/* }}} */

/* {{{ proto bool http_match_etag(string etag[, for_range = false])
 *
 * This matches the given ETag against the clients
 * "If-Match" resp. "If-None-Match" HTTP headers.
 *
 */
PHP_FUNCTION(http_match_etag)
{
	int etag_len;
	char *etag;
	zend_bool for_range = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &etag, &etag_len, &for_range) != SUCCESS) {
		RETURN_FALSE;
	}

	if (for_range) {
		RETURN_BOOL(http_match_etag("HTTP_IF_MATCH", etag));
	}
	RETURN_BOOL(http_match_etag("HTTP_IF_NONE_MATCH", etag));
}
/* }}} */

/* {{{ proto bool http_cache_last_modified([int timestamp_or_expires]])
 *
 * If timestamp_or_expires is greater than 0, it is handled as timestamp
 * and will be sent as date of last modification.  If it is 0 or omitted,
 * the current time will be sent as Last-Modified date.  If it's negative,
 * it is handled as expiration time in seconds, which means that if the
 * requested last modification date is not between the calculated timespan,
 * the Last-Modified header is updated and the actual body will be sent.
 *
 */
PHP_FUNCTION(http_cache_last_modified)
{
	long last_modified = 0, send_modified = 0, t;
	zval *zlm;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &last_modified) != SUCCESS) {
		RETURN_FALSE;
	}

	t = (long) time(NULL);

	/* 0 or omitted */
	if (!last_modified) {
		/* does the client have? (att: caching "forever") */
		if (zlm = http_get_server_var("HTTP_IF_MODIFIED_SINCE")) {
			last_modified = send_modified = http_parse_date(Z_STRVAL_P(zlm));
		/* send current time */
		} else {
			send_modified = t;
		}
	/* negative value is supposed to be expiration time */
	} else if (last_modified < 0) {
		last_modified += t;
		send_modified  = t;
	/* send supplied time explicitly */
	} else {
		send_modified = last_modified;
	}

	RETURN_SUCCESS(http_cache_last_modified(last_modified, send_modified, HTTP_DEFAULT_CACHECONTROL, lenof(HTTP_DEFAULT_CACHECONTROL)));
}
/* }}} */

/* {{{ proto bool http_cache_etag([string etag])
 *
 * This function attempts to cache the HTTP body based on an ETag,
 * either supplied or generated through calculation of the MD5
 * checksum of the output (uses output buffering).
 *
 * If clients "If-None-Match" header matches the supplied/calculated
 * ETag, the body is considered cached on the clients side and
 * a "304 Not Modified" status code is issued.
 *
 */
PHP_FUNCTION(http_cache_etag)
{
	char *etag = NULL;
	int etag_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &etag, &etag_len) != SUCCESS) {
		RETURN_FALSE;
	}

	RETURN_SUCCESS(http_cache_etag(etag, etag_len, HTTP_DEFAULT_CACHECONTROL, lenof(HTTP_DEFAULT_CACHECONTROL)));
}
/* }}} */

/* {{{ proto string ob_etaghandler(string data, int mode)
 *
 * For use with ob_start().
 */
PHP_FUNCTION(ob_etaghandler)
{
	char *data;
	int data_len;
	long mode;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &data, &data_len, &mode)) {
		RETURN_FALSE;
	}

	Z_TYPE_P(return_value) = IS_STRING;
	http_ob_etaghandler(data, data_len, &Z_STRVAL_P(return_value), &Z_STRLEN_P(return_value), mode);
}
/* }}} */

/* {{{ proto void http_redirect([string url[, array params[, bool session,[ bool permanent]]]])
 *
 * Redirect to a given url.
 * The supplied url will be expanded with http_absolute_uri(), the params array will
 * be treated with http_build_query() and the session identification will be appended
 * if session is true.
 *
 * Depending on permanent the redirection will be issued with a permanent
 * ("301 Moved Permanently") or a temporary ("302 Found") redirection
 * status code.
 *
 * To be RFC compliant, "Redirecting to <a>URI</a>." will be displayed,
 * if the client doesn't redirect immediatly.
 */
PHP_FUNCTION(http_redirect)
{
	int url_len;
	size_t query_len = 0;
	zend_bool session = 0, permanent = 0;
	zval *params = NULL;
	char *query, *url, *URI,
		LOC[HTTP_URI_MAXLEN + sizeof("Location: ")],
		RED[HTTP_URI_MAXLEN * 2 + sizeof("Redirecting to <a href=\"%s?%s\">%s?%s</a>.\n")];

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sa!/bb", &url, &url_len, &params, &session, &permanent) != SUCCESS) {
		RETURN_FALSE;
	}

	/* append session info */
	if (session && (PS(session_status) == php_session_active)) {
		if (!params) {
			MAKE_STD_ZVAL(params);
			array_init(params);
		}
		if (add_assoc_string(params, PS(session_name), PS(id), 1) != SUCCESS) {
			http_error(E_WARNING, HTTP_E_ENCODE, "Could not append session information");
		}
	}

	/* treat params array with http_build_query() */
	if (params) {
		if (SUCCESS != http_urlencode_hash_ex(Z_ARRVAL_P(params), 0, NULL, 0, &query, &query_len)) {
			RETURN_FALSE;
		}
	}

	URI = http_absolute_uri(url);

	if (query_len) {
		snprintf(LOC, HTTP_URI_MAXLEN + sizeof("Location: "), "Location: %s?%s", URI, query);
		sprintf(RED, "Redirecting to <a href=\"%s?%s\">%s?%s</a>.\n", URI, query, URI, query);
		efree(query);
	} else {
		snprintf(LOC, HTTP_URI_MAXLEN + sizeof("Location: "), "Location: %s", URI);
		sprintf(RED, "Redirecting to <a href=\"%s\">%s</a>.\n", URI, URI);
	}
	efree(URI);

	if ((SUCCESS == http_send_header(LOC)) && (SUCCESS == http_send_status((permanent ? 301 : 302)))) {
		php_body_write(RED, strlen(RED) TSRMLS_CC);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool http_send_data(string data)
 *
 * Sends raw data with support for (multiple) range requests.
 *
 */
PHP_FUNCTION(http_send_data)
{
	zval *zdata;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zdata) != SUCCESS) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&zdata);
	RETURN_SUCCESS(http_send_data(Z_STRVAL_P(zdata), Z_STRLEN_P(zdata)));
}
/* }}} */

/* {{{ proto bool http_send_file(string file)
 *
 * Sends a file with support for (multiple) range requests.
 *
 */
PHP_FUNCTION(http_send_file)
{
	char *file;
	int flen = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &file, &flen) != SUCCESS) {
		RETURN_FALSE;
	}
	if (!flen) {
		RETURN_FALSE;
	}

	RETURN_SUCCESS(http_send_file(file));
}
/* }}} */

/* {{{ proto bool http_send_stream(resource stream)
 *
 * Sends an already opened stream with support for (multiple) range requests.
 *
 */
PHP_FUNCTION(http_send_stream)
{
	zval *zstream;
	php_stream *file;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zstream) != SUCCESS) {
		RETURN_FALSE;
	}

	php_stream_from_zval(file, &zstream);
	RETURN_SUCCESS(http_send_stream(file));
}
/* }}} */

/* {{{ proto string http_chunked_decode(string encoded)
 *
 * This function decodes a string that was HTTP-chunked encoded.
 * Returns false on failure.
 */
PHP_FUNCTION(http_chunked_decode)
{
	char *encoded = NULL, *decoded = NULL;
	int encoded_len = 0, decoded_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &encoded, &encoded_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (NULL != http_chunked_decode(encoded, encoded_len, &decoded, &decoded_len)) {
		RETURN_STRINGL(decoded, decoded_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto array http_split_response(string http_response)
 *
 * This function splits an HTTP response into an array with headers and the
 * content body. The returned array may look simliar to the following example:
 *
 * <pre>
 * <?php
 * array(
 *     0 => array(
 *         'Response Status' => '200 Ok',
 *         'Content-Type' => 'text/plain',
 *         'Content-Language' => 'en-US'
 *     ),
 *     1 => "Hello World!"
 * );
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_split_response)
{
	zval *zresponse, *zbody, *zheaders;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zresponse) != SUCCESS) {
		RETURN_FALSE;
	}

	convert_to_string(zresponse);

	MAKE_STD_ZVAL(zbody);
	MAKE_STD_ZVAL(zheaders);
	array_init(zheaders);

	if (SUCCESS != http_split_response(zresponse, zheaders, zbody)) {
		http_error(E_WARNING, HTTP_E_PARSE, "Could not parse HTTP response");
		RETURN_FALSE;
	}

	array_init(return_value);
	add_index_zval(return_value, 0, zheaders);
	add_index_zval(return_value, 1, zbody);
}
/* }}} */

/* {{{ proto array http_parse_headers(string header)
 *
 */
PHP_FUNCTION(http_parse_headers)
{
	char *header;
	int header_len;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &header, &header_len)) {
		RETURN_FALSE;
	}

	array_init(return_value);
	if (SUCCESS != http_parse_headers(header, return_value)) {
		http_error(E_WARNING, HTTP_E_PARSE, "Could not parse HTTP headers");
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}}*/

/* {{{ proto array http_get_request_headers(void)
 *
 */
PHP_FUNCTION(http_get_request_headers)
{
	NO_ARGS;

	array_init(return_value);
	http_get_request_headers(return_value);
}
/* }}} */

/* {{{ HAVE_CURL */
#ifdef HTTP_HAVE_CURL

/* {{{ proto string http_get(string url[, array options[, array &info]])
 *
 * Performs an HTTP GET request on the supplied url.
 *
 * The second parameter is expected to be an associative
 * array where the following keys will be recognized:
 * <pre>
 *  - redirect:         int, whether and how many redirects to follow
 *  - unrestrictedauth: bool, whether to continue sending credentials on
 *                      redirects to a different host
 *  - proxyhost:        string, proxy host in "host[:port]" format
 *  - proxyport:        int, use another proxy port as specified in proxyhost
 *  - proxyauth:        string, proxy credentials in "user:pass" format
 *  - proxyauthtype:    int, HTTP_AUTH_BASIC and/or HTTP_AUTH_NTLM
 *  - httpauth:         string, http credentials in "user:pass" format
 *  - httpauthtype:     int, HTTP_AUTH_BASIC, DIGEST and/or NTLM
 *  - compress:         bool, whether to allow gzip/deflate content encoding
 *                      (defaults to true)
 *  - port:             int, use another port as specified in the url
 *  - referer:          string, the referer to sends
 *  - useragent:        string, the user agent to send
 *                      (defaults to PECL::HTTP/version (PHP/version)))
 *  - headers:          array, list of custom headers as associative array
 *                      like array("header" => "value")
 *  - cookies:          array, list of cookies as associative array
 *                      like array("cookie" => "value")
 *  - cookiestore:      string, path to a file where cookies are/will be stored
 *  - resume:           int, byte offset to start the download from;
 *                      if the server supports ranges
 *  - maxfilesize:      int, maximum file size that should be downloaded;
 *                      has no effect, if the size of the requested entity is not known
 *  - lastmodified:     int, timestamp for If-(Un)Modified-Since header
 *  - timeout:          int, seconds the request may take
 *  - connecttimeout:   int, seconds the connect may take
 *  - onprogress:       mixed, progress callback
 *  - ondebug:          mixed, debug callback
 * </pre>
 *
 * The optional third parameter will be filled with some additional information
 * in form af an associative array, if supplied, like the following example:
 * <pre>
 * <?php
 * array (
 *     'effective_url' => 'http://localhost',
 *     'response_code' => 403,
 *     'total_time' => 0.017,
 *     'namelookup_time' => 0.013,
 *     'connect_time' => 0.014,
 *     'pretransfer_time' => 0.014,
 *     'size_upload' => 0,
 *     'size_download' => 202,
 *     'speed_download' => 11882,
 *     'speed_upload' => 0,
 *     'header_size' => 145,
 *     'request_size' => 62,
 *     'ssl_verifyresult' => 0,
 *     'filetime' => -1,
 *     'content_length_download' => 202,
 *     'content_length_upload' => 0,
 *     'starttransfer_time' => 0.017,
 *     'content_type' => 'text/html; charset=iso-8859-1',
 *     'redirect_time' => 0,
 *     'redirect_count' => 0,
 *     'private' => '',
 *     'http_connectcode' => 0,
 *     'httpauth_avail' => 0,
 *     'proxyauth_avail' => 0,
 * )
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_get)
{
	zval *options = NULL, *info = NULL;
	char *URL;
	int URL_len;
	phpstr response;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a/!z", &URL, &URL_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	phpstr_init_ex(&response, HTTP_CURLBUF_SIZE, 0);
	if (SUCCESS == http_get(URL, options ? Z_ARRVAL_P(options) : NULL, info ? Z_ARRVAL_P(info) : NULL, &response)) {
		RETURN_PHPSTR_VAL(response);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string http_head(string url[, array options[, array &info]])
 *
 * Performs an HTTP HEAD request on the suppied url.
 * Returns the HTTP response as string.
 * See http_get() for a full list of available options.
 */
PHP_FUNCTION(http_head)
{
	zval *options = NULL, *info = NULL;
	char *URL;
	int URL_len;
	phpstr response;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a/!z", &URL, &URL_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	phpstr_init_ex(&response, HTTP_CURLBUF_SIZE, 0);
	if (SUCCESS == http_head(URL, options ? Z_ARRVAL_P(options) : NULL, info ? Z_ARRVAL_P(info) : NULL, &response)) {
		RETURN_PHPSTR_VAL(response);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string http_post_data(string url, string data[, array options[, &info]])
 *
 * Performs an HTTP POST request, posting data.
 * Returns the HTTP response as string.
 * See http_get() for a full list of available options.
 */
PHP_FUNCTION(http_post_data)
{
	zval *options = NULL, *info = NULL;
	char *URL, *postdata;
	int postdata_len, URL_len;
	phpstr response;
	http_request_body body;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a/!z", &URL, &URL_len, &postdata, &postdata_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	body.type = HTTP_REQUEST_BODY_CSTRING;
	body.data = postdata;
	body.size = postdata_len;

	phpstr_init_ex(&response, HTTP_CURLBUF_SIZE, 0);
	if (SUCCESS == http_post(URL, &body, options ? Z_ARRVAL_P(options) : NULL, info ? Z_ARRVAL_P(info) : NULL, &response)) {
		RETVAL_PHPSTR_VAL(response);
	} else {
		RETVAL_FALSE;
	}
	http_request_body_dtor(&body);
}
/* }}} */

/* {{{ proto string http_post_fields(string url, array data[, array files[, array options[, array &info]]])
 *
 * Performs an HTTP POST request, posting www-form-urlencoded array data.
 * Returns the HTTP response as string.
 * See http_get() for a full list of available options.
 */
PHP_FUNCTION(http_post_fields)
{
	zval *options = NULL, *info = NULL, *fields, *files = NULL;
	char *URL;
	int URL_len;
	phpstr response;
	http_request_body body;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|aa/!z", &URL, &URL_len, &fields, &files, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (SUCCESS != http_request_body_fill(&body, Z_ARRVAL_P(fields), files ? Z_ARRVAL_P(files) : NULL)) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	phpstr_init_ex(&response, HTTP_CURLBUF_SIZE, 0);
	if (SUCCESS == http_post(URL, &body, options ? Z_ARRVAL_P(options) : NULL, info ? Z_ARRVAL_P(info) : NULL, &response)) {
		RETVAL_PHPSTR_VAL(response);
	} else {
		RETVAL_FALSE;
	}
	http_request_body_dtor(&body);
}
/* }}} */

#endif
/* }}} HAVE_CURL */


/* {{{ proto bool http_auth_basic(string user, string pass[, string realm = "Restricted"])
 *
 * Example:
 * <pre>
 * <?php
 * if (!http_auth_basic('mike', 's3c|r3t')) {
 *     die('<h1>Authorization failed!</h1>');
 * }
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_auth_basic)
{
	char *realm = NULL, *user, *pass, *suser, *spass;
	int r_len, u_len, p_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|s", &user, &u_len, &pass, &p_len, &realm, &r_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!realm) {
		realm = "Restricted";
	}

	if (SUCCESS != http_auth_credentials(&suser, &spass)) {
		http_auth_header("Basic", realm);
		RETURN_FALSE;
	}

	if (strcasecmp(suser, user)) {
		http_auth_header("Basic", realm);
		RETURN_FALSE;
	}

	if (strcmp(spass, pass)) {
		http_auth_header("Basic", realm);
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool http_auth_basic_cb(mixed callback[, string realm = "Restricted"])
 *
 * Example:
 * <pre>
 * <?php
 * function auth_cb($user, $pass)
 * {
 *     global $db;
 *     $query = 'SELECT pass FROM users WHERE user='. $db->quoteSmart($user);
 *     if (strlen($realpass = $db->getOne($query)) {
 *         return $pass === $realpass;
 *     }
 *     return false;
 * }
 * if (!http_auth_basic_cb('auth_cb')) {
 *     die('<h1>Authorization failed</h1>');
 * }
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_auth_basic_cb)
{
	zval *cb;
	char *realm = NULL, *user, *pass;
	int r_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|s", &cb, &realm, &r_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!realm) {
		realm = "Restricted";
	}

	if (SUCCESS != http_auth_credentials(&user, &pass)) {
		http_auth_header("Basic", realm);
		RETURN_FALSE;
	}
	{
		zval *zparams[2] = {NULL, NULL}, retval;
		int result = 0;

		MAKE_STD_ZVAL(zparams[0]);
		MAKE_STD_ZVAL(zparams[1]);
		ZVAL_STRING(zparams[0], user, 0);
		ZVAL_STRING(zparams[1], pass, 0);

		if (SUCCESS == call_user_function(EG(function_table), NULL, cb,
				&retval, 2, zparams TSRMLS_CC)) {
			result = Z_LVAL(retval);
		}

		efree(user);
		efree(pass);
		efree(zparams[0]);
		efree(zparams[1]);

		if (!result) {
			http_auth_header("Basic", realm);
		}

		RETURN_BOOL(result);
	}
}
/* }}}*/

/* {{{ Sara Golemons http_build_query() */
#ifndef ZEND_ENGINE_2

/* {{{ proto string http_build_query(mixed formdata [, string prefix[, string arg_separator]])
   Generates a form-encoded query string from an associative array or object. */
PHP_FUNCTION(http_build_query)
{
	zval *formdata;
	char *prefix = NULL, *arg_sep = INI_STR("arg_separator.output");
	int prefix_len = 0, arg_sep_len = strlen(arg_sep);
	phpstr *formstr;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|ss", &formdata, &prefix, &prefix_len, &arg_sep, &arg_sep_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (Z_TYPE_P(formdata) != IS_ARRAY && Z_TYPE_P(formdata) != IS_OBJECT) {
		http_error(E_WARNING, HTTP_E_PARAM, "Parameter 1 expected to be Array or Object.  Incorrect value given.");
		RETURN_FALSE;
	}

	if (!arg_sep_len) {
		arg_sep = HTTP_URL_ARGSEP;
	}

	formstr = phpstr_new();
	if (SUCCESS != http_urlencode_hash_implementation_ex(HASH_OF(formdata), formstr, arg_sep, prefix, prefix_len, NULL, 0, NULL, 0, (Z_TYPE_P(formdata) == IS_OBJECT ? formdata : NULL))) {
		phpstr_free(formstr);
		RETURN_FALSE;
	}

	if (!formstr->used) {
		phpstr_free(formstr);
		RETURN_NULL();
	}

	RETURN_PHPSTR_PTR(formstr);
}
/* }}} */
#endif /* !ZEND_ENGINE_2 */
/* }}} */

PHP_FUNCTION(http_test)
{
	RETURN_NULL();
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
