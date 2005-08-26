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
#include "ext/standard/php_string.h"
#if defined(HAVE_PHP_SESSION) && !defined(COMPILE_DL_SESSION)
#	include "ext/session/php_session.h"
#endif

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_request_api.h"
#include "php_http_cache_api.h"
#include "php_http_request_api.h"
#include "php_http_date_api.h"
#include "php_http_headers_api.h"
#include "php_http_message_api.h"
#include "php_http_send_api.h"
#include "php_http_url_api.h"

#include "phpstr/phpstr.h"

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
		http_error_ex(HE_WARNING, HTTP_E_HEADER, "Invalid HTTP status code (100-510): %d", status);
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

/* {{{ proto void http_throttle(double sec[, long bytes = 2097152])
 *
 * Use with http_send() API.
 *
 * Example:
 * <pre>
 * <?php
 * // ~ 20 kbyte/s
 * # http_throttle(1, 20000);
 * # http_throttle(0.5, 10000);
 * # http_throttle(0.1, 2000);
 * http_send_file('document.pdf');
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_throttle)
{
	long chunk_size;
	double interval;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "dl", &interval, &chunk_size)) {
		return;
	}

	HTTP_G(send).throttle_delay = interval;
	HTTP_G(send).buffer_size = chunk_size;
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
	zend_bool session = 0, permanent = 0, free_params = 0;
	zval *params = NULL;
	char *query = NULL, *url = NULL, *URI, *LOC, *RED = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sa!/bb", &url, &url_len, &params, &session, &permanent) != SUCCESS) {
		RETURN_FALSE;
	}

	/* append session info */
	if (session) {
		if (!params) {
			free_params = 1;
			MAKE_STD_ZVAL(params);
			array_init(params);
		}
#ifdef HAVE_PHP_SESSION
#	ifdef COMPILE_DL_SESSION
		if (SUCCESS == zend_get_module_started("session")) {
			zval nm_retval, id_retval, func;
			
			INIT_PZVAL(&func);
			INIT_PZVAL(&nm_retval);
			INIT_PZVAL(&id_retval);
			ZVAL_NULL(&nm_retval);
			ZVAL_NULL(&id_retval);
			
			ZVAL_STRINGL(&func, "session_id", lenof("session_id"), 0);
			call_user_function(EG(function_table), NULL, &func, &id_retval, 0, NULL TSRMLS_CC);
			ZVAL_STRINGL(&func, "session_name", lenof("session_name"), 0);
			call_user_function(EG(function_table), NULL, &func, &nm_retval, 0, NULL TSRMLS_CC);
			
			if (	Z_TYPE(nm_retval) == IS_STRING && Z_STRLEN(nm_retval) &&
					Z_TYPE(id_retval) == IS_STRING && Z_STRLEN(id_retval)) {
				if (add_assoc_stringl_ex(params, Z_STRVAL(nm_retval), Z_STRLEN(nm_retval)+1, 
						Z_STRVAL(id_retval), Z_STRLEN(id_retval), 0) != SUCCESS) {
					http_error(HE_WARNING, HTTP_E_RUNTIME, "Could not append session information");
				}
			}
		}
#	else
		if (PS(session_status) == php_session_active) {
			if (add_assoc_string(params, PS(session_name), PS(id), 1) != SUCCESS) {
				http_error(HE_WARNING, HTTP_E_RUNTIME, "Could not append session information");
			}
		}
#	endif
#endif
	}

	/* treat params array with http_build_query() */
	if (params) {
		if (SUCCESS != http_urlencode_hash_ex(Z_ARRVAL_P(params), 0, NULL, 0, &query, &query_len)) {
			if (free_params) {
				zval_dtor(params);
				FREE_ZVAL(params);
			}
			if (query) {
				efree(query);
			}
			RETURN_FALSE;
		}
	}

	URI = http_absolute_uri(url);

	if (query_len) {
		spprintf(&LOC, 0, "Location: %s?%s", URI, query);
		if (SG(request_info).request_method && strcmp(SG(request_info).request_method, "HEAD")) {
			spprintf(&RED, 0, "Redirecting to <a href=\"%s?%s\">%s?%s</a>.\n", URI, query, URI, query);
		}
	} else {
		spprintf(&LOC, 0, "Location: %s", URI);
		if (SG(request_info).request_method && strcmp(SG(request_info).request_method, "HEAD")) {
			spprintf(&RED, 0, "Redirecting to <a href=\"%s\">%s</a>.\n", URI, URI);
		}
	}
	
	efree(URI);
	if (query) {
		efree(query);
	}
	if (free_params) {
		zval_dtor(params);
		FREE_ZVAL(params);
	}

	RETURN_SUCCESS(http_exit_ex(permanent ? 301 : 302, LOC, RED, 1));
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

/* {{{ proto object http_parse_message(string message)
 *
 * Parses (a) http_message(s) into a simple recursive object structure:
 * 
 * <pre>
 * <?php
 * print_r(http_parse_message(http_get(URL, array('redirect' => 3)));
 * 
 * stdClass object
 * (
 *     [type] => 2
 *     [httpVersion] => 1.1
 *     [responseCode] => 200
 *     [headers] => Array 
 *         (
 *             [Content-Length] => 3
 *             [Server] => Apache
 *         )
 *     [body]  => Hi!
 *     [parentMessage] => stdClass object
 *     (
 *         [type] => 2
 *         [httpVersion] => 1.1
 *         [responseCode] => 302
 *         [headers] => Array 
 *             (
 *                 [Content-Length] => 0
 *                 [Location] => ...
 *             )
 *         [body]  => 
 *         [parentMessage] => ...
 *     )
 * )
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_parse_message)
{
	char *message;
	int message_len;
	http_message *msg = NULL;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &message, &message_len)) {
		RETURN_NULL();
	}
	
	
	if (msg = http_message_parse(message, message_len)) {
		object_init(return_value);
		http_message_tostruct_recursive(msg, return_value);
		http_message_free(&msg);
	} else {
		RETURN_NULL();
	}
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
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}}*/

/* {{{ proto array http_get_request_headers(void)
 *
 * Get a list of incoming HTTP headers.
 */
PHP_FUNCTION(http_get_request_headers)
{
	NO_ARGS;

	array_init(return_value);
	http_get_request_headers(return_value);
}
/* }}} */

/* {{{ proto string http_get_request_body(void)
 *
 * Get the raw request body (e.g. POST or PUT data).
 */
PHP_FUNCTION(http_get_request_body)
{
	char *body;
	size_t length;

	NO_ARGS;

	if (SUCCESS == http_get_request_body(&body, &length)) {
		RETURN_STRINGL(body, (int) length, 0);
	} else {
		RETURN_NULL();
	}
}
/* }}} */

/* {{{ proto bool http_match_request_header(string header, string value[, bool match_case = false])
 *
 * Match an incoming HTTP header.
 */
PHP_FUNCTION(http_match_request_header)
{
	char *header, *value;
	int header_len, value_len;
	zend_bool match_case = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|b", &header, &header_len, &value, &value_len, &match_case)) {
		RETURN_FALSE;
	}

	RETURN_BOOL(http_match_request_header_ex(header, value, match_case));
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
		RETURN_PHPSTR_VAL(&response);
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
		RETURN_PHPSTR_VAL(&response);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string http_post_data(string url, string data[, array options[, array &info]])
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
		RETVAL_PHPSTR_VAL(&response);
	} else {
		RETVAL_FALSE;
	}
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
		RETVAL_PHPSTR_VAL(&response);
	} else {
		RETVAL_FALSE;
	}
	http_request_body_dtor(&body);
}
/* }}} */

/* {{{ proto string http_put_file(string url, string file[, array options[, array &info]])
 *
 * Performs an HTTP PUT request, uploading file.
 * Returns the HTTP response as string.
 * See http_get() for a full list of available options.
 */
PHP_FUNCTION(http_put_file)
{
	char *URL, *file;
	int URL_len, f_len;
	zval *options = NULL, *info = NULL;
	phpstr response;
	php_stream *stream;
	php_stream_statbuf ssb;
	http_request_body body;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a/!z", &URL, &URL_len, &file, &f_len, &options, &info)) {
		RETURN_FALSE;
	}

	if (!(stream = php_stream_open_wrapper(file, "rb", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL))) {
		RETURN_FALSE;
	}
	if (php_stream_stat(stream, &ssb)) {
		php_stream_close(stream);
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	body.type = HTTP_REQUEST_BODY_UPLOADFILE;
	body.data = stream;
	body.size = ssb.sb.st_size;

	phpstr_init_ex(&response, HTTP_CURLBUF_SIZE, 0);
	if (SUCCESS == http_put(URL, &body, options ? Z_ARRVAL_P(options) : NULL, info ? Z_ARRVAL_P(info) : NULL, &response)) {
		RETVAL_PHPSTR_VAL(&response);
	} else {
		RETVAL_FALSE;
	}
	http_request_body_dtor(&body);
}
/* }}} */

/* {{{ proto string http_put_stream(string url, resource stream[, array options[, array &info]])
 *
 * Performs an HTTP PUT request, uploading stream.
 * Returns the HTTP response as string.
 * See http_get() for a full list of available options.
 */
PHP_FUNCTION(http_put_stream)
{
	zval *resource, *options = NULL, *info = NULL;
	char *URL;
	int URL_len;
	phpstr response;
	php_stream *stream;
	php_stream_statbuf ssb;
	http_request_body body;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sr|a/!z", &URL, &URL_len, &resource, &options, &info)) {
		RETURN_FALSE;
	}

	php_stream_from_zval(stream, &resource);
	if (php_stream_stat(stream, &ssb)) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	body.type = HTTP_REQUEST_BODY_UPLOADFILE;
	body.data = stream;
	body.size = ssb.sb.st_size;

	phpstr_init_ex(&response, HTTP_CURLBUF_SIZE, 0);
	if (SUCCESS == http_put(URL, &body, options ? Z_ARRVAL_P(options) : NULL, info ? Z_ARRVAL_P(info) : NULL, &response)) {
		RETURN_PHPSTR_VAL(&response);
	} else {
		RETURN_NULL();
	}
}
/* }}} */

/* {{{ proto long http_request_method_register(string method)
 *
 * Register a custom request method.
 */
PHP_FUNCTION(http_request_method_register)
{
	char *method;
	int *method_len;
	unsigned long existing;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &method, &method_len)) {
		RETURN_FALSE;
	}
	if (existing = http_request_method_exists(1, 0, method)) {
		RETURN_LONG((long) existing);
	}

	RETVAL_LONG((long) http_request_method_register(method));
}
/* }}} */

/* {{{ proto bool http_request_method_unregister(mixed method)
 *
 * Unregister a previously registered custom request method.
 */
PHP_FUNCTION(http_request_method_unregister)
{
	zval *method;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &method)) {
		RETURN_FALSE;
	}

	switch (Z_TYPE_P(method))
	{
		case IS_OBJECT:
			convert_to_string(method);
		case IS_STRING:
#include "zend_operators.h"
			if (is_numeric_string(Z_STRVAL_P(method), Z_STRLEN_P(method), NULL, NULL, 1)) {
				convert_to_long(method);
			} else {
				unsigned long mn;
				if (!(mn = http_request_method_exists(1, 0, Z_STRVAL_P(method)))) {
					RETURN_FALSE;
				}
				zval_dtor(method);
				ZVAL_LONG(method, (long)mn);
			}
		case IS_LONG:
			RETURN_SUCCESS(http_request_method_unregister(Z_LVAL_P(method)));
		default:
			RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto long http_request_method_exists(mixed method)
 *
 * Check if a request method is registered (or available by default).
 */
PHP_FUNCTION(http_request_method_exists)
{
	IF_RETVAL_USED {
		zval *method;

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &method)) {
			RETURN_FALSE;
		}

		switch (Z_TYPE_P(method))
		{
			case IS_OBJECT:
				convert_to_string(method);
			case IS_STRING:
				if (is_numeric_string(Z_STRVAL_P(method), Z_STRLEN_P(method), NULL, NULL, 1)) {
					convert_to_long(method);
				} else {
					RETURN_LONG((long) http_request_method_exists(1, 0, Z_STRVAL_P(method)));
				}
			case IS_LONG:
				RETURN_LONG((long) http_request_method_exists(0, Z_LVAL_P(method), NULL));
			default:
				RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto string http_request_method_name(long method)
 *
 * Get the literal string representation of a standard or registered request method.
 */
PHP_FUNCTION(http_request_method_name)
{
	IF_RETVAL_USED {
		long method;

		if ((SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &method)) || (method < 0)) {
			RETURN_FALSE;
		}

		RETURN_STRING(estrdup(http_request_method_name((unsigned long) method)), 0);
	}
}
/* }}} */
#endif
/* }}} HAVE_CURL */

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
		http_error(HE_WARNING, HTTP_E_INVALID_PARAM, "Parameter 1 expected to be Array or Object.  Incorrect value given.");
		RETURN_FALSE;
	}

	if (!arg_sep_len) {
		arg_sep = HTTP_URL_ARGSEP;
	}

	formstr = phpstr_new();
	if (SUCCESS != http_urlencode_hash_implementation_ex(HASH_OF(formdata), formstr, arg_sep, prefix, prefix_len, NULL, 0, NULL, 0, (Z_TYPE_P(formdata) == IS_OBJECT ? formdata : NULL))) {
		phpstr_free(&formstr);
		RETURN_FALSE;
	}

	if (!formstr->used) {
		phpstr_free(&formstr);
		RETURN_NULL();
	}

	RETURN_PHPSTR_PTR(formstr);
}
/* }}} */
#endif /* !ZEND_ENGINE_2 */
/* }}} */

PHP_FUNCTION(http_test)
{
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

