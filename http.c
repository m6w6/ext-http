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

#define ZEND_INCLUDE_FULL_WINDOWS_HEADERS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "snprintf.h"
#include "ext/standard/info.h"
#include "ext/session/php_session.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_smart_str.h"

#include "php_http.h"
#include "php_http_api.h"

#ifdef ZEND_ENGINE_2
#include "ext/standard/php_http.h"
#endif

#ifdef HTTP_HAVE_CURL

#ifdef PHP_WIN32
#include <winsock2.h>
#include <sys/types.h>
#endif

#include <curl/curl.h>
#endif

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
	PHP_FE(http_match_modified, NULL)
	PHP_FE(http_match_etag, NULL)
	PHP_FE(http_cache_last_modified, NULL)
	PHP_FE(http_cache_etag, NULL)
	PHP_FE(http_content_type, NULL)
	PHP_FE(http_content_disposition, NULL)
	PHP_FE(http_send_data, NULL)
	PHP_FE(http_send_file, NULL)
	PHP_FE(http_send_stream, NULL)
	PHP_FE(http_chunked_decode, NULL)
	PHP_FE(http_split_response, NULL)
	PHP_FE(http_parse_headers, NULL)
#ifdef HTTP_HAVE_CURL
	PHP_FE(http_get, NULL)
	PHP_FE(http_head, NULL)
	PHP_FE(http_post_data, NULL)
	PHP_FE(http_post_array, NULL)
#endif
	PHP_FE(http_auth_basic, NULL)
	PHP_FE(http_auth_basic_cb, NULL)
#ifndef ZEND_ENGINE_2
	PHP_FE(http_build_query, NULL)
#endif
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
	NULL,
	NULL,
	PHP_RSHUTDOWN(http),
	PHP_MINFO(http),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_EXT_HTTP_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#define RETURN_SUCCESS(v) RETURN_BOOL(SUCCESS == (v))
#define HASH_ORNULL(z) ((z) ? Z_ARRVAL_P(z) : NULL)

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

/* {{{ proto string http_absolute_uri(string url[, string proto])
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
	char *url = NULL, *proto = NULL;
	int url_len = 0, proto_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &url, &url_len, &proto, &proto_len) != SUCCESS) {
		RETURN_FALSE;
	}

	RETURN_STRING(http_absolute_uri(url, proto), 0);
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
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid HTTP status code (100-510): %d", status);
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

/* {{{ proto bool http_match_modified([int timestamp])
 *
 * Matches the given timestamp against the clients "If-Modified-Since" resp.
 * "If-Unmodified-Since" HTTP headers.
 *
 */
PHP_FUNCTION(http_match_modified)
{
	long t = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &t) != SUCCESS) {
		RETURN_FALSE;
	}

	// current time if not supplied (senseless though)
	if (t == -1) {
		t = (long) time(NULL);
	}

	RETURN_BOOL(http_modified_match("HTTP_IF_MODIFIED_SINCE", t) || http_modified_match("HTTP_IF_UNMODIFIED_SINCE", t));
}
/* }}} */

/* {{{ proto bool http_match_etag(string etag)
 *
 * This matches the given ETag against the clients
 * "If-Match" resp. "If-None-Match" HTTP headers.
 *
 */
PHP_FUNCTION(http_match_etag)
{
	int etag_len;
	char *etag;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &etag, &etag_len) != SUCCESS) {
		RETURN_FALSE;
	}

	RETURN_BOOL(http_etag_match("HTTP_IF_NONE_MATCH", etag) || http_etag_match("HTTP_IF_MATCH", etag));
}
/* }}} */

/* {{{ proto bool http_cache_last_modified([int timestamp_or_expires]])
 *
 * If timestamp_or_exires is greater than 0, it is handled as timestamp
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

	http_send_header("Cache-Control: private, must-revalidate, max-age=0");

	if (http_modified_match("HTTP_IF_MODIFIED_SINCE", last_modified)) {
		if (SUCCESS == http_send_status(304)) {
			zend_bailout();
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not send 304 Not Modified");
			RETURN_FALSE;
		}
	}
	RETURN_SUCCESS(http_send_last_modified(send_modified));
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
	char *etag;
	int etag_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &etag, &etag_len) != SUCCESS) {
		RETURN_FALSE;
	}

	php_end_ob_buffers(0 TSRMLS_CC);
	http_send_header("Cache-Control: private, must-revalidate, max-age=0");

	/* if no etag is given and we didn't already
	 * start ob_etaghandler -- start it
	 */
	if (!HTTP_G(etag_started) && !etag_len) {
		php_ob_set_internal_handler(_http_ob_etaghandler, (uint) 4096, "etag output handler", 0 TSRMLS_CC);
		HTTP_G(etag_started) = 1;
		RETURN_BOOL(php_start_ob_buffer_named("etag output handler", (uint) 4096, 0 TSRMLS_CC));
	}

	if (http_etag_match("HTTP_IF_NONE_MATCH", etag)) {
		if (SUCCESS == http_send_status(304)) {
			zend_bailout();
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not send 304 Not Modified");
			RETURN_FALSE;
		}
	}

	RETURN_SUCCESS(http_send_etag(etag, etag_len));
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
	zend_bool session = 0, permanent = 0;
	zval *params = NULL;
	smart_str qstr = {0};
	char *url, *URI, LOC[HTTP_URI_MAXLEN + 9], RED[HTTP_URI_MAXLEN * 2 + 34];

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
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not append session information");
		}
	}

	/* treat params array with http_build_query() */
	if (params) {
		if (php_url_encode_hash_ex(Z_ARRVAL_P(params), &qstr, NULL,0,NULL,0,NULL,0,NULL TSRMLS_CC) != SUCCESS) {
			if (qstr.c) {
				efree(qstr.c);
			}
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not encode query parameters");
			RETURN_FALSE;
		}
		smart_str_0(&qstr);
	}

	URI = http_absolute_uri(url, NULL);
	if (qstr.c) {
		snprintf(LOC, HTTP_URI_MAXLEN + strlen("Location: "), "Location: %s?%s", URI, qstr.c);
		sprintf(RED, "Redirecting to <a href=\"%s?%s\">%s?%s</a>.\n", URI, qstr.c, URI, qstr.c);
		efree(qstr.c);
	} else {
		snprintf(LOC, HTTP_URI_MAXLEN + strlen("Location: "), "Location: %s", URI);
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
	http_send_header("Accept-Ranges: bytes");
	RETURN_SUCCESS(http_send_data(zdata));
}
/* }}} */

/* {{{ proto bool http_send_file(string file)
 *
 * Sends a file with support for (multiple) range requests.
 *
 */
PHP_FUNCTION(http_send_file)
{
	zval *zfile;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zfile) != SUCCESS) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&zfile);
	http_send_header("Accept-Ranges: bytes");
	RETURN_SUCCESS(http_send_file(zfile));
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
	http_send_header("Accept-Ranges: bytes");
	RETURN_SUCCESS(http_send_stream(file));
}
/* }}} */

/* {{{ proto bool http_content_type([string content_type = 'application/x-octetstream'])
 *
 * Sets the content type.
 *
 */
PHP_FUNCTION(http_content_type)
{
	char *ct, *content_type;
	int ct_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &ct, &ct_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!ct_len) {
		RETURN_SUCCESS(http_send_header("Content-Type: application/x-octetstream"));
	}

	/* remember for multiple ranges */
	if (HTTP_G(ctype)) {
		efree(HTTP_G(ctype));
	}
	HTTP_G(ctype) = estrndup(ct, ct_len);

	content_type = (char *) emalloc(strlen("Content-Type: ") + ct_len + 1);
	sprintf(content_type, "Content-Type: %s", ct);

	RETVAL_BOOL(SUCCESS == http_send_header(content_type));
	efree(content_type);
}
/* }}} */

/* {{{ proto bool http_content_disposition(string filename[, bool inline = false])
 *
 * Set the Content Disposition.  The Content-Disposition header is very useful
 * if the data actually sent came from a file or something similar, that should
 * be "saved" by the client/user (i.e. by browsers "Save as..." popup window).
 *
 */
PHP_FUNCTION(http_content_disposition)
{
	char *filename, *header;
	int f_len;
	zend_bool send_inline = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &filename, &f_len, &send_inline) != SUCCESS) {
		RETURN_FALSE;
	}

	if (send_inline) {
		header = (char *) emalloc(strlen("Content-Disposition: inline; filename=\"\"") + f_len + 1);
		sprintf(header, "Content-Disposition: inline; filename=\"%s\"", filename);
	} else {
		header = (char *) emalloc(strlen("Content-Disposition: attachment; filename=\"\"") + f_len + 1);
		sprintf(header, "Content-Disposition: attachment; filename=\"%s\"", filename);
	}

	RETVAL_BOOL(SUCCESS == http_send_header(header));
	efree(header);
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

	if (SUCCESS == http_chunked_decode(encoded, encoded_len, &decoded, &decoded_len)) {
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
 * array(
 *     0 => array(
 *         'Status' => '200 Ok',
 *         'Content-Type' => 'text/plain',
 *         'Content-Language' => 'en-US'
 *     ),
 *     1 => "Hello World!"
 * );
 * </pre>
 */
PHP_FUNCTION(http_split_response)
{
	zval *zresponse, *zbody, *zheaders;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zresponse) != SUCCESS) {
		RETURN_FALSE;
	}

	convert_to_string_ex(&zresponse);

	MAKE_STD_ZVAL(zbody);
	MAKE_STD_ZVAL(zheaders);
	array_init(zheaders);

	if (SUCCESS != http_split_response(zresponse, zheaders, zbody)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not parse HTTP response");
		RETURN_FALSE;
	}

	array_init(return_value);
	add_index_zval(return_value, 0, zheaders);
	add_index_zval(return_value, 1, zbody);
}
/* }}} */

/* {{{ proto array http_parse_headers(string header) */
PHP_FUNCTION(http_parse_headers)
{
	char *header, *rnrn;
	int header_len;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &header, &header_len)) {
		RETURN_FALSE;
	}

	array_init(return_value);

	if (rnrn = strstr(header, HTTP_CRLF HTTP_CRLF)) {
		header_len = rnrn - header + 2;
	}
	if (SUCCESS != http_parse_headers(header, header_len, return_value)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not parse HTTP header");
		zval_dtor(return_value);
		RETURN_FALSE;
	}
}
/* }}}*/

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
 * </pre>
 *
 * The optional third parameter will be filled with some additional information
 * in form af an associative array, if supplied (don't forget to initialize it
 * with NULL or array()).
 */
PHP_FUNCTION(http_get)
{
	char *URL, *data = NULL;
	size_t data_len = 0;
	int URL_len;
	zval *options = NULL, *info = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a/!z", &URL, &URL_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	if (SUCCESS == http_get(URL, HASH_ORNULL(options), HASH_ORNULL(info), &data, &data_len)) {
		RETURN_STRINGL(data, data_len, 0);
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
	char *URL, *data = NULL;
	size_t data_len = 0;
	int URL_len;
	zval *options = NULL, *info = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a/!z", &URL, &URL_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	if (SUCCESS == http_head(URL, HASH_ORNULL(options), HASH_ORNULL(info), &data, &data_len)) {
		RETURN_STRINGL(data, data_len, 0);
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
	char *URL, *postdata, *data = NULL;
	size_t data_len = 0;
	int postdata_len, URL_len;
	zval *options = NULL, *info = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a/!z", &URL, &URL_len, &postdata, &postdata_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	if (SUCCESS == http_post_data(URL, postdata, (size_t) postdata_len, HASH_ORNULL(options), HASH_ORNULL(info), &data, &data_len)) {
		RETURN_STRINGL(data, data_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string http_post_array(string url, array data[, array options[, array &info]])
 *
 * Performs an HTTP POST request, posting www-form-urlencoded array data.
 * Returns the HTTP response as string.
 * See http_get() for a full list of available options.
 */
PHP_FUNCTION(http_post_array)
{
	char *URL, *data = NULL;
	size_t data_len = 0;
	int URL_len;
	zval *options = NULL, *info = NULL, *postdata;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|a/!z", &URL, &URL_len, &postdata, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	if (SUCCESS == http_post_array(URL, Z_ARRVAL_P(postdata), HASH_ORNULL(options), HASH_ORNULL(info), &data, &data_len)) {
		RETURN_STRINGL(data, data_len, 0);
	} else {
		RETURN_FALSE;
	}
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
 *
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


/* {{{ php_http_init_globals(zend_http_globals *) */
static void php_http_init_globals(zend_http_globals *http_globals)
{
	http_globals->etag_started = 0;
	http_globals->ctype = NULL;
	http_globals->etag  = NULL;
	http_globals->lmod  = 0;
#ifdef HTTP_HAVE_CURL
	http_globals->curlbuf.body.data = NULL;
	http_globals->curlbuf.body.used = 0;
	http_globals->curlbuf.body.free = 0;
	http_globals->curlbuf.hdrs.data = NULL;
	http_globals->curlbuf.hdrs.used = 0;
	http_globals->curlbuf.hdrs.free = 0;
#endif
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(http)
{
	ZEND_INIT_MODULE_GLOBALS(http, php_http_init_globals, NULL);
#ifdef HTTP_HAVE_CURL
	REGISTER_LONG_CONSTANT("HTTP_AUTH_BASIC", CURLAUTH_BASIC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_AUTH_DIGEST", CURLAUTH_DIGEST, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("HTTP_AUTH_NTLM", CURLAUTH_NTLM, CONST_CS | CONST_PERSISTENT);
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(http)
{
	if (HTTP_G(ctype)) {
		efree(HTTP_G(ctype));
	}
	if (HTTP_G(etag)) {
		efree(HTTP_G(etag));
	}
#ifdef HTTP_HAVE_CURL
	if (HTTP_G(curlbuf).body.data) {
		efree(HTTP_G(curlbuf).body.data);
	}
	if (HTTP_G(curlbuf).hdrs.data) {
		efree(HTTP_G(curlbuf).hdrs.data);
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

