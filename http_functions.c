/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2006, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_SAPI
#define HTTP_WANT_CURL
#define HTTP_WANT_ZLIB
#include "php_http.h"

#include "php_ini.h"
#include "ext/standard/php_string.h"
#include "zend_operators.h"

#ifdef HAVE_PHP_SESSION && !HTTP_SHARED_EXT(PHP_SESSION)
#	include "ext/session/php_session.h"
#endif

#include "php_http_api.h"
#include "php_http_cache_api.h"
#include "php_http_cookie_api.h"
#include "php_http_date_api.h"
#include "php_http_encoding_api.h"
#include "php_http_headers_api.h"
#include "php_http_message_api.h"
#include "php_http_request_api.h"
#include "php_http_request_method_api.h"
#include "php_http_send_api.h"
#include "php_http_url_api.h"

/* {{{ proto string http_date([int timestamp])
 *
 * Compose a valid HTTP date regarding RFC 1123
 * looking like: "Wed, 22 Dec 2004 11:34:47 GMT"
 *
 * Accepts an optional unix timestamp as parameter.
 *
 * Returns the HTTP date as string.
 */
PHP_FUNCTION(http_date)
{
	long t = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &t) != SUCCESS) {
		RETURN_FALSE;
	}

	if (t == -1) {
		t = HTTP_G->request.time;
	}

	RETURN_STRING(http_date(t), 0);
}
/* }}} */

/* {{{ proto string http_build_url([mixed url[, mixed parts[, int flags = HTTP_URL_REPLACE[, array &new_url]]]])
 *
 * Build an URL.
 *
 * Expexts (part(s) of) an URL as first parameter in form of a string or assoziative array
 * like parse_url() returns.  Accepts an optional second parameter in the same way as the
 * first argument.  Accepts an optional third integer parameter, which is a bitmask of
 * binary or'ed HTTP_URL_* constants.  The optional fourth parameter will be filled
 * with the results as associative array like parse_url() would return.
 *
 * The parts of the second URL will be merged into the first according to the flags argument.
 * The following flags are recognized:
 * <pre>
 *	- HTTP_URL_REPLACE:        (default) set parts of the second url will replace the parts in the first
 *	- HTTP_URL_JOIN_PATH:      the path of the second url will be merged into the one of the first
 *	- HTTP_URL_JOIN_QUERY:     the two querystrings will be merged recursively
 *	- HTTP_URL_STRIP_USER:     the user part will not appear in the result
 *	- HTTP_URL_STRIP_PASS:     the password part will not appear in the result
 *	- HTTP_URL_STRIP_AUTH:     neither the user nor the password part will appear in the result
 *	- HTTP_URL_STRIP_PORT:     no explicit port will be set in the result
 *	- HTTP_URL_STRIP_PATH:     the path part will not appear in the result
 *	- HTTP_URL_STRIP_QUERY:    no query string will be present in the result
 *	- HTTP_URL_STRIP_FRAGMENT: no fragment will be present in the result
 * </pre>
 *
 * Example:
 * <pre>
 * <?php
 * 		// ftp://ftp.example.com/pub/files/current/?a=b&a=c
 * 		echo http_build_url("http://user@www.example.com/pub/index.php?a=b#files",
 * 			array(
 * 				"scheme" => "ftp",
 * 				"host"   => "ftp.example.com",
 * 				"path"   => "files/current/",
 * 				"query"  => "a=c"
 * 			),
 * 			HTTP_URL_STRIP_AUTH | HTTP_URL_JOIN_PATH | HTTP_URL_JOIN_QUERY | HTTP_URL_STRIP_FRAGMENT
 * 		);
 * ?>
 * </pre>
 * Returns the new URL as string on success or FALSE on failure.
 */
PHP_FUNCTION(http_build_url)
{
	char *url_str = NULL;
	size_t url_len = 0;
	long flags = HTTP_URL_REPLACE;
	zval *z_old_url = NULL, *z_new_url = NULL, *z_composed_url = NULL;
	php_url *old_url = NULL, *new_url = NULL, *composed_url = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z!/z!/lz", &z_old_url, &z_new_url, &flags, &z_composed_url) != SUCCESS) {
		RETURN_FALSE;
	}
	
	if (z_new_url) {
		if (Z_TYPE_P(z_new_url) == IS_ARRAY || Z_TYPE_P(z_new_url) == IS_OBJECT) {
			new_url = http_url_from_struct(NULL, HASH_OF(z_new_url));
		} else {
			convert_to_string(z_new_url);
			if (!(new_url = php_url_parse_ex(Z_STRVAL_P(z_new_url), Z_STRLEN_P(z_new_url)))) {
				http_error_ex(HE_WARNING, HTTP_E_URL, "Could not parse URL (%s)", Z_STRVAL_P(z_new_url));
				RETURN_FALSE;
			}
		}
	}
	
	if (z_old_url) {
		if (Z_TYPE_P(z_old_url) == IS_ARRAY || Z_TYPE_P(z_old_url) == IS_OBJECT) {
			old_url = http_url_from_struct(NULL, HASH_OF(z_old_url));
		} else {
			convert_to_string(z_old_url);
			if (!(old_url = php_url_parse_ex(Z_STRVAL_P(z_old_url), Z_STRLEN_P(z_old_url)))) {
				if (new_url) {
					php_url_free(new_url);
				}
				http_error_ex(HE_WARNING, HTTP_E_URL, "Could not parse URL (%s)", Z_STRVAL_P(z_old_url));
				RETURN_FALSE;
			}
		}
	}
	
	if (z_composed_url) {
		http_build_url(flags, old_url, new_url, &composed_url, &url_str, &url_len);
		http_url_tostruct(composed_url, z_composed_url);
		php_url_free(composed_url);
	} else {
		http_build_url(flags, old_url, new_url, NULL, &url_str, &url_len);
	}
	
	if (new_url) {
		php_url_free(new_url);
	}
	if (old_url) {
		php_url_free(old_url);
	}
	
	RETURN_STRINGL(url_str, url_len, 0);
}
/* }}} */

/* {{{ proto string http_build_str(array query [, string prefix[, string arg_separator]])
 *
 * Opponent to parse_str().
 *
 * Expects an array as first argument which represents the parts of the query string to build.
 * Accepts a string as optional second parameter containing a top-level prefix to use.
 * The optional third parameter should specify an argument separator to use (by default the
 * INI setting arg_separator.output will be used, or "&" if neither is set).
 *
 * Returns the built query as string on success or FALSE on failure.
 */
PHP_FUNCTION(http_build_str)
{
	zval *formdata;
	char *prefix = NULL, *arg_sep = INI_STR("arg_separator.output");
	int prefix_len = 0, arg_sep_len = strlen(arg_sep);
	phpstr formstr;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|ss", &formdata, &prefix, &prefix_len, &arg_sep, &arg_sep_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!arg_sep_len) {
		arg_sep = HTTP_URL_ARGSEP;
		arg_sep_len = lenof(HTTP_URL_ARGSEP);
	}

	phpstr_init(&formstr);
	if (SUCCESS != http_urlencode_hash_recursive(HASH_OF(formdata), &formstr, arg_sep, arg_sep_len, prefix, prefix_len)) {
		RETURN_FALSE;
	}

	if (!formstr.used) {
		phpstr_dtor(&formstr);
		RETURN_NULL();
	}

	RETURN_PHPSTR_VAL(&formstr);
}
/* }}} */

#define HTTP_DO_NEGOTIATE(type, supported, rs_array) \
{ \
	HashTable *result; \
	if ((result = http_negotiate_ ##type(supported))) { \
		char *key; \
		uint key_len; \
		ulong idx; \
		 \
		if (HASH_KEY_IS_STRING == zend_hash_get_current_key_ex(result, &key, &key_len, &idx, 1, NULL)) { \
			RETVAL_STRINGL(key, key_len-1, 0); \
		} else { \
			RETVAL_NULL(); \
		} \
		\
		if (rs_array) { \
			zend_hash_copy(Z_ARRVAL_P(rs_array), result, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *)); \
		} \
		\
		zend_hash_destroy(result); \
		FREE_HASHTABLE(result); \
		\
	} else { \
		zval **value; \
		 \
		zend_hash_internal_pointer_reset(Z_ARRVAL_P(supported)); \
		if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(supported), (void *) &value)) { \
			RETVAL_ZVAL(*value, 1, 0); \
		} else { \
			RETVAL_NULL(); \
		} \
		\
		if (rs_array) { \
			HashPosition pos; \
			zval **value; \
			 \
			FOREACH_VAL(pos, supported, value) { \
				convert_to_string_ex(value); \
				add_assoc_double(rs_array, Z_STRVAL_PP(value), 1.0); \
			} \
		} \
	} \
}


/* {{{ proto string http_negotiate_language(array supported[, array &result])
 *
 * This function negotiates the clients preferred language based on its
 * Accept-Language HTTP header.  The qualifier is recognized and languages 
 * without qualifier are rated highest.  The qualifier will be decreased by
 * 10% for partial matches (i.e. matching primary language).
 * 
 * Expects an array as parameter containing the supported languages as values.
 * If the optional second parameter is supplied, it will be filled with an
 * array containing the negotiation results.
 * 
 * Returns the negotiated language or the default language (i.e. first array entry) 
 * if none match.
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
 * 
 * include './langs/'. http_negotiate_language($langs, $result) .'.php';
 * 
 * print_r($result);
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_negotiate_language)
{
	zval *supported, *rs_array = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|z", &supported, &rs_array) != SUCCESS) {
		RETURN_FALSE;
	}
	
	if (rs_array) {
		zval_dtor(rs_array);
		array_init(rs_array);
	}
	
	HTTP_DO_NEGOTIATE(language, supported, rs_array);
}
/* }}} */

/* {{{ proto string http_negotiate_charset(array supported[, array &result])
 *
 * This function negotiates the clients preferred charset based on its
 * Accept-Charset HTTP header.  The qualifier is recognized and charsets 
 * without qualifier are rated highest.
 * 
 * Expects an array as parameter containing the supported charsets as values.
 * If the optional second parameter is supplied, it will be filled with an
 * array containing the negotiation results.
 * 
 * Returns the negotiated charset or the default charset (i.e. first array entry) 
 * if none match.
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
 * 
 * $pref = http_negotiate_charset($charsets, $result);
 * 
 * if (strcmp($pref, 'iso-8859-1')) {
 * 		iconv_set_encoding('internal_encoding', 'iso-8859-1');
 * 		iconv_set_encoding('output_encoding', $pref);
 * 		ob_start('ob_iconv_handler');
 * }
 * 
 * print_r($result);
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_negotiate_charset)
{
	zval *supported, *rs_array = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|z", &supported, &rs_array) != SUCCESS) {
		RETURN_FALSE;
	}
	
	if (rs_array) {
		zval_dtor(rs_array);
		array_init(rs_array);
	}

	HTTP_DO_NEGOTIATE(charset, supported, rs_array);
}
/* }}} */

/* {{{ proto string http_negotiate_ctype(array supported[, array &result])
 *
 * This function negotiates the clients preferred content type based on its
 * Accept HTTP header.  The qualifier is recognized and content types 
 * without qualifier are rated highest.
 * 
 * Expects an array as parameter containing the supported content types as values.
 * If the optional second parameter is supplied, it will be filled with an
 * array containing the negotiation results.
 * 
 * Returns the negotiated content type or the default content type 
 * (i.e. first array entry) if none match.
 * 
 * Example:
 * <pre>
 * <?php
 * $ctypes = array('application/xhtml+xml', 'text/html');
 * http_send_content_type(http_negotiate_content_type($ctypes));
 * ?>
 * </pre>
 */
PHP_FUNCTION(http_negotiate_content_type)
{
	zval *supported, *rs_array = NULL;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|z", &supported, &rs_array)) {
		RETURN_FALSE;
	}
	
	if (rs_array) {
		zval_dtor(rs_array);
		array_init(rs_array);
	}
	
	HTTP_DO_NEGOTIATE(content_type, supported, rs_array);
}
/* }}} */

/* {{{ proto bool http_send_status(int status)
 *
 * Send HTTP status code.
 *
 * Expects an HTTP status code as parameter.
 * 
 * Returns TRUE on success or FALSE on failure.
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
 * Send a "Last-Modified" header with a valid HTTP date.
 * 
 * Accepts a unix timestamp, converts it to a valid HTTP date and
 * sends it as "Last-Modified" HTTP header.  If timestamp is
 * omitted, the current time will be sent.
 *
 * Returns TRUE on success or FALSE on failure.
 */
PHP_FUNCTION(http_send_last_modified)
{
	long t = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &t) != SUCCESS) {
		RETURN_FALSE;
	}

	if (t == -1) {
		t = HTTP_G->request.time;
	}

	RETURN_SUCCESS(http_send_last_modified(t));
}
/* }}} */

/* {{{ proto bool http_send_content_type([string content_type = 'application/x-octetstream'])
 *
 * Send the Content-Type of the sent entity.  This is particularly important
 * if you use the http_send() API.
 * 
 * Accepts an optional string parameter containing the desired content type 
 * (primary/secondary).
 *
 * Returns TRUE on success or FALSE on failure.
 */
PHP_FUNCTION(http_send_content_type)
{
	char *ct = "application/x-octetstream";
	int ct_len = lenof("application/x-octetstream");

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &ct, &ct_len) != SUCCESS) {
		RETURN_FALSE;
	}

	RETURN_SUCCESS(http_send_content_type(ct, ct_len));
}
/* }}} */

/* {{{ proto bool http_send_content_disposition(string filename[, bool inline = false])
 *
 * Send the Content-Disposition.  The Content-Disposition header is very useful
 * if the data actually sent came from a file or something similar, that should
 * be "saved" by the client/user (i.e. by browsers "Save as..." popup window).
 *
 * Expects a string parameter specifying the file name the "Save as..." dialog
 * should display.  Optionally accepts a bool parameter, which, if set to true
 * and the user agent knows how to handle the content type, will probably not
 * cause the popup window to be shown.
 * 
 * Returns TRUE on success or FALSE on failure.
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

/* {{{ proto bool http_match_modified([int timestamp[, bool for_range = false]])
 *
 * Matches the given unix timestamp against the clients "If-Modified-Since" 
 * resp. "If-Unmodified-Since" HTTP headers.
 *
 * Accepts a unix timestamp which should be matched.  Optionally accepts an
 * additional bool parameter, which if set to true will check the header 
 * usually used to validate HTTP ranges.  If timestamp is omitted, the
 * current time will be used.
 * 
 * Returns TRUE if timestamp represents an earlier date than the header,
 * else FALSE.
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
		t = HTTP_G->request.time;
	}

	if (for_range) {
		RETURN_BOOL(http_match_last_modified("HTTP_IF_UNMODIFIED_SINCE", t));
	}
	RETURN_BOOL(http_match_last_modified("HTTP_IF_MODIFIED_SINCE", t));
}
/* }}} */

/* {{{ proto bool http_match_etag(string etag[, bool for_range = false])
 *
 * Matches the given ETag against the clients "If-Match" resp. 
 * "If-None-Match" HTTP headers.
 *
 * Expects a string parameter containing the ETag to compare.  Optionally
 * accepts a bool parameter, which, if set to true, will check the header
 * usually used to validate HTTP ranges.
 * 
 * Returns TRUE if ETag matches or the header contained the asterisk ("*"),
 * else FALSE.
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
 * Attempts to cache the sent entity by its last modification date.
 * 
 * Accepts a unix timestamp as parameter which is handled as follows:
 * 
 * If timestamp_or_expires is greater than 0, it is handled as timestamp
 * and will be sent as date of last modification.  If it is 0 or omitted,
 * the current time will be sent as Last-Modified date.  If it's negative,
 * it is handled as expiration time in seconds, which means that if the
 * requested last modification date is not between the calculated timespan,
 * the Last-Modified header is updated and the actual body will be sent.
 *
 * Returns FALSE on failure, or *exits* with "304 Not Modified" if the entity is cached.
 * 
 * A log entry will be written to the cache log if the INI entry
 * http.log.cache is set and the cache attempt was successful.
 */
PHP_FUNCTION(http_cache_last_modified)
{
	long last_modified = 0, send_modified = 0, t;
	zval *zlm;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &last_modified) != SUCCESS) {
		RETURN_FALSE;
	}
	
	HTTP_CHECK_HEADERS_SENT(RETURN_FALSE);

	t = HTTP_G->request.time;

	/* 0 or omitted */
	if (!last_modified) {
		/* does the client have? (att: caching "forever") */
		if ((zlm = http_get_server_var("HTTP_IF_MODIFIED_SINCE"))) {
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
 * Attempts to cache the sent entity by its ETag, either supplied or generated 
 * by the hash algorithm specified by the INI setting "http.etag.mode".
 *
 * If the clients "If-None-Match" header matches the supplied/calculated
 * ETag, the body is considered cached on the clients side and
 * a "304 Not Modified" status code is issued.
 *
 * Returns FALSE on failure, or *exits* with "304 Not Modified" if the entity is cached.
 * 
 * A log entry is written to the cache log if the INI entry
 * "http.log.cache" is set and the cache attempt was successful.
 */
PHP_FUNCTION(http_cache_etag)
{
	char *etag = NULL;
	int etag_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &etag, &etag_len) != SUCCESS) {
		RETURN_FALSE;
	}
	
	HTTP_CHECK_HEADERS_SENT(RETURN_FALSE);

	RETURN_SUCCESS(http_cache_etag(etag, etag_len, HTTP_DEFAULT_CACHECONTROL, lenof(HTTP_DEFAULT_CACHECONTROL)));
}
/* }}} */

/* {{{ proto string ob_etaghandler(string data, int mode)
 *
 * For use with ob_start().  Output buffer handler generating an ETag with
 * the hash algorithm specified with the INI setting "http.etag.mode".
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
	http_ob_etaghandler(data, data_len, &Z_STRVAL_P(return_value), (uint *) &Z_STRLEN_P(return_value), mode);
}
/* }}} */

/* {{{ proto void http_throttle(double sec[, int bytes = 40960])
 *
 * Sets the throttle delay and send buffer size for use with http_send() API.
 * Provides a basic throttling mechanism, which will yield the current process
 * resp. thread until the entity has been completely sent, though.
 * 
 * Expects a double parameter specifying the seconds too sleep() after
 * each chunk sent.  Additionally accepts an optional int parameter
 * representing the chunk size in bytes.
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
	long chunk_size = HTTP_SENDBUF_SIZE;
	double interval;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "d|l", &interval, &chunk_size)) {
		return;
	}

	HTTP_G->send.throttle_delay = interval;
	HTTP_G->send.buffer_size = chunk_size;
}
/* }}} */

/* {{{ proto void http_redirect([string url[, array params[, bool session = false[, int status = 302]]]])
 *
 * Redirect to the given url.
 *  
 * The supplied url will be expanded with http_build_url(), the params array will
 * be treated with http_build_query() and the session identification will be appended
 * if session is true.
 *
 * The HTTP response code will be set according to status.
 * You can use one of the following constants for convenience:
 *  - HTTP_REDIRECT			302 Found for GET/HEAD, else 303 See Other
 *  - HTTP_REDIRECT_PERM	301 Moved Permanently
 *  - HTTP_REDIRECT_FOUND	302 Found
 *  - HTTP_REDIRECT_POST	303 See Other
 *  - HTTP_REDIRECT_PROXY	305 Use Proxy
 *  - HTTP_REDIRECT_TEMP	307 Temporary Redirect
 *
 * Please see http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html#sec10.3
 * for which redirect response code to use in which situation.
 *
 * To be RFC compliant, "Redirecting to <a>URL</a>." will be displayed,
 * if the client doesn't redirect immediately, and the request method was
 * another one than HEAD.
 * 
 * Returns FALSE on failure, or *exits* on success.
 * 
 * A log entry will be written to the redirect log, if the INI entry
 * "http.log.redirect" is set and the redirect attempt was successful.
 */
PHP_FUNCTION(http_redirect)
{
	int url_len = 0;
	size_t query_len = 0;
	zend_bool session = 0, free_params = 0;
	zval *params = NULL;
	long status = HTTP_REDIRECT;
	char *query = NULL, *url = NULL, *URI, *LOC, *RED = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sa!/bl", &url, &url_len, &params, &session, &status) != SUCCESS) {
		RETURN_FALSE;
	}

#ifdef HAVE_PHP_SESSION && !HTTP_SHARED_EXT(PHP_SESSION)
	/* append session info */
	if (session) {
		if (!params) {
			free_params = 1;
			MAKE_STD_ZVAL(params);
			array_init(params);
		}
		if (PS(session_status) == php_session_active) {
			if (add_assoc_string(params, PS(session_name), PS(id), 1) != SUCCESS) {
				http_error(HE_WARNING, HTTP_E_RUNTIME, "Could not append session information");
			}
		}
	}
#endif

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

	URI = http_absolute_url(url);

	if (query_len) {
		spprintf(&LOC, 0, "Location: %s?%s", URI, query);
		if (status != 300) {
			spprintf(&RED, 0, "Redirecting to <a href=\"%s?%s\">%s?%s</a>.\n", URI, query, URI, query);
		}
	} else {
		spprintf(&LOC, 0, "Location: %s", URI);
		if (status != 300) {
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
	
	switch (status) {
		case 300:
			RETVAL_SUCCESS(http_send_status_header(status, LOC));
			efree(LOC);
			return;
		
		case HTTP_REDIRECT_PERM:
		case HTTP_REDIRECT_FOUND:
		case HTTP_REDIRECT_POST:
		case HTTP_REDIRECT_PROXY:
		case HTTP_REDIRECT_TEMP:
			break;
		
		case 306:
		default:
			http_error_ex(HE_NOTICE, HTTP_E_RUNTIME, "Unsupported redirection status code: %ld", status);
		case HTTP_REDIRECT:
			if (	SG(request_info).request_method && 
					strcasecmp(SG(request_info).request_method, "HEAD") &&
					strcasecmp(SG(request_info).request_method, "GET")) {
				status = HTTP_REDIRECT_POST;
			} else {
				status = HTTP_REDIRECT_FOUND;
			}
			break;
	}
	
	RETURN_SUCCESS(http_exit_ex(status, LOC, RED, 1));
}
/* }}} */

/* {{{ proto bool http_send_data(string data)
 *
 * Sends raw data with support for (multiple) range requests.
 *
 * Returns TRUE on success, or FALSE on failure.
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
 * Expects a string parameter referencing the file to send.
 * 
 * Returns TRUE on success, or FALSE on failure.
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
 * Expects a resource parameter referencing the stream to read from.
 * 
 * Returns TRUE on success, or FALSE on failure.
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
 * Decodes a string that was HTTP-chunked encoded.
 * 
 * Expects a chunked encoded string as parameter.
 * 
 * Returns the decoded string on success or FALSE on failure.
 */
PHP_FUNCTION(http_chunked_decode)
{
	char *encoded = NULL, *decoded = NULL;
	size_t decoded_len = 0;
	int encoded_len = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &encoded, &encoded_len) != SUCCESS) {
		RETURN_FALSE;
	}

	if (NULL != http_encoding_dechunk(encoded, encoded_len, &decoded, &decoded_len)) {
		RETURN_STRINGL(decoded, (int) decoded_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto object http_parse_message(string message)
 *
 * Parses (a) http_message(s) into a simple recursive object structure.
 * 
 * Expects a string parameter containing a single HTTP message or
 * several consecutive HTTP messages.
 * 
 * Returns an hierarchical object structure of the parsed messages.
 *
 * Example:
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
	
	if ((msg = http_message_parse(message, message_len))) {
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
 * Parses HTTP headers into an associative array.
 * 
 * Expects a string parameter containing HTTP headers.
 * 
 * Returns an array on success, or FALSE on failure.
 * 
 * Example:
 * <pre>
 * <?php
 * $headers = "content-type: text/html; charset=UTF-8\r\n".
 *            "Server: Funky/1.0\r\n".
 *            "Set-Cookie: foo=bar\r\n".
 *            "Set-Cookie: baz=quux\r\n".
 *            "Folded: works\r\n\ttoo\r\n";
 * print_r(http_parse_headers($headers));
 * 
 * Array
 * (
 *     [Content-Type] => text/html; chatset=UTF-8
 *     [Server] => Funky/1.0
 *     [Set-Cookie] => Array
 *         (
 *             [0] => foo=bar
 *             [1] => baz=quux
 *         )
 *     [Folded] => works
 *         too 
 * ) 
 * ?>
 * </pre>
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
		http_error(HE_WARNING, HTTP_E_MALFORMED_HEADERS, "Failed to parse headers");
		RETURN_FALSE;
	}
}
/* }}}*/

/* {{{ proto object http_parse_cookie(string cookie[, int flags[, array allowed_extras]])
 *
 * Parses HTTP cookies like sent in a response into a struct.
 * 
 * Expects a string as parameter containing the value of a Set-Cookie response header.
 * 
 * Returns an stdClass olike shown in the example on success or FALSE on failure.
 * 
 * Example:
 * <pre>
 * <?php
 * print_r(http_parse_cookie("foo=bar; bar=baz; path=/; domain=example.com; comment=; secure", 0, array("comment")));
 * 
 * stdClass Object
 * (
 *     [cookies] => Array
 *         (
 *             [foo] => bar
 *             [bar] => baz
 *         )
 * 
 *     [extras] => Array
 *         (
 *             [comment] =>
 *         )
 * 
 *     [flags] => 16
 *     [expires] => 0
 *     [path] => /
 *     [domain] => example.com
 * )
 * ?>
 * </pre> 
 */
PHP_FUNCTION(http_parse_cookie)
{
	char *cookie, **allowed_extras = NULL;
	int i = 0, cookie_len;
	long flags = 0;
	zval *allowed_extras_array = NULL, **entry = NULL;
	HashPosition pos;
	http_cookie_list list;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|la", &cookie, &cookie_len, &flags, &allowed_extras_array)) {
		RETURN_FALSE;
	}
	
	if (allowed_extras_array) {
		allowed_extras = ecalloc(zend_hash_num_elements(Z_ARRVAL_P(allowed_extras_array)) + 1, sizeof(char *));
		FOREACH_VAL(pos, allowed_extras_array, entry) {
			ZVAL_ADDREF(*entry);
			convert_to_string_ex(entry);
			allowed_extras[i] = estrndup(Z_STRVAL_PP(entry), Z_STRLEN_PP(entry));
			zval_ptr_dtor(entry);
		}
	}
	
	if (http_parse_cookie_ex(&list, cookie, flags, allowed_extras)) {
		object_init(return_value);
		http_cookie_list_tostruct(&list, return_value);
		http_cookie_list_dtor(&list);
	} else {
		RETVAL_FALSE;
	}
	
	if (allowed_extras) {
		for (i = 0; allowed_extras[i]; ++i) {
			efree(allowed_extras[i]);
		}
		efree(allowed_extras);
	}
}

/* {{{ proto object http_parse_params(string param[, int flags = HTTP_PARAMS_DEFAULT])
 *
 * Parse parameter list.
 */
PHP_FUNCTION(http_parse_params)
{
	char *param;
	int param_len;
	zval *params;
	long flags = HTTP_PARAMS_DEFAULT;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &param, &param_len, &flags)) {
		RETURN_FALSE;
	}
	
	params = ecalloc(1, sizeof(zval));
	array_init(params);
	if (SUCCESS != http_parse_params(param, flags, Z_ARRVAL_P(params))) {
		zval_dtor(params);
		FREE_ZVAL(params);
		RETURN_FALSE;
	}
	object_init(return_value);
	add_property_zval(return_value, "params", params);
}
/* }}} */

/* {{{ proto array http_get_request_headers(void)
 *
 * Get a list of incoming HTTP headers.
 * 
 * Returns an associative array of incoming request headers.
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
 * 
 * This function can not be used after http_get_request_body_stream() 
 * if the request method was another than POST.
 * 
 * Returns the raw request body as string on success or NULL on failure.
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

/* {{{ proto resource http_get_request_body_stream(void)
 *
 * Create a stream to read the raw request body (e.g. POST or PUT data).
 * 
 * This function can only be used once if the request method was another than POST.
 * 
 * Returns the raw request body as stream on success or NULL on failure.
 */
PHP_FUNCTION(http_get_request_body_stream)
{
	php_stream *s;
	
	NO_ARGS;
	
	if ((s = http_get_request_body_stream())) {
		php_stream_to_zval(s, return_value);
	} else {
		http_error(HE_WARNING, HTTP_E_RUNTIME, "Failed to create request body stream");
		RETURN_NULL();
	}
}
/* }}} */

/* {{{ proto bool http_match_request_header(string header, string value[, bool match_case = false])
 *
 * Match an incoming HTTP header.
 * 
 * Expects two string parameters representing the header name (case-insensitive)
 * and the header value that should be compared.  The case sensitivity of the
 * header value depends on the additional optional bool parameter accepted.
 * 
 * Returns TRUE if header value matches, else FALSE.
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

#define RETVAL_RESPONSE_OR_BODY(request) \
	{ \
		zval **bodyonly; \
		 \
		/* check if only the body should be returned */ \
		if (options && (SUCCESS == zend_hash_find(Z_ARRVAL_P(options), "bodyonly", sizeof("bodyonly"), (void *) &bodyonly)) && zval_is_true(*bodyonly)) { \
			http_message *msg = http_message_parse(PHPSTR_VAL(&request.conv.response), PHPSTR_LEN(&request.conv.response)); \
			 \
			if (msg) { \
				RETVAL_STRINGL(PHPSTR_VAL(&msg->body), PHPSTR_LEN(&msg->body), 1); \
				http_message_free(&msg); \
			} \
		} else { \
			RETVAL_STRINGL(request.conv.response.data, request.conv.response.used, 1); \
		} \
	}

/* {{{ proto string http_get(string url[, array options[, array &info]])
 *
 * Performs an HTTP GET request on the supplied url.
 *
 * The second parameter, if set, is expected to be an associative
 * array where the following keys will be recognized:
 * <pre>
 *  - redirect:         int, whether and how many redirects to follow
 *  - unrestrictedauth: bool, whether to continue sending credentials on
 *                      redirects to a different host
 *  - proxyhost:        string, proxy host in "host[:port]" format
 *  - proxyport:        int, use another proxy port as specified in proxyhost
 *  - proxytype:        int, HTTP_PROXY_HTTP, SOCKS4 or SOCKS5
 *  - proxyauth:        string, proxy credentials in "user:pass" format
 *  - proxyauthtype:    int, HTTP_AUTH_BASIC and/or HTTP_AUTH_NTLM
 *  - httpauth:         string, http credentials in "user:pass" format
 *  - httpauthtype:     int, HTTP_AUTH_BASIC, DIGEST and/or NTLM
 *  - compress:         bool, whether to allow gzip/deflate content encoding
 *  - port:             int, use another port as specified in the url
 *  - referer:          string, the referer to send
 *  - useragent:        string, the user agent to send
 *                      (defaults to PECL::HTTP/version (PHP/version)))
 *  - headers:          array, list of custom headers as associative array
 *                      like array("header" => "value")
 *  - cookies:          array, list of cookies as associative array
 *                      like array("cookie" => "value")
 *  - encodecookies:    bool, whether to urlencode the cookies (default: true)
 *  - cookiestore:      string, path to a file where cookies are/will be stored
 *  - cookiesession:    bool, don't load session cookies from cookiestore if TRUE
 *  - resume:           int, byte offset to start the download from;
 *                      if the server supports ranges
 *  - range:            array, array of arrays, each containing two integers,
 *                      specifying the ranges to download if server support is
 *                      given; only recognized if the resume option is empty
 *  - maxfilesize:      int, maximum file size that should be downloaded;
 *                      has no effect, if the size of the requested entity is not known
 *  - lastmodified:     int, timestamp for If-(Un)Modified-Since header
 *  - etag:             string, quoted etag for If-(None-)Match header
 *  - timeout:          int, seconds the request may take
 *  - connecttimeout:   int, seconds the connect may take
 *  - onprogress:       mixed, progress callback
 *  - interface:        string, outgoing network interface (ifname, ip or hostname)
 *  - portrange:        array, 2 integers specifying outgoing portrange to try
 *  - ssl:              array, with the following options:
 *                      cert:        string, path to certificate
 *                      certtype:    string, type of certificate
 *                      certpasswd:  string, password for certificate
 *                      key:         string, path to key
 *                      keytype:     string, type of key
 *                      keypasswd:   string, pasword for key
 *                      engine:      string, ssl engine to use
 *                      version:     int, ssl version to use
 *                      verifypeer:  bool, whether to verify the peer
 *                      verifyhost:  bool whether to verify the host
 *                      cipher_list: string, list of allowed ciphers
 *                      cainfo:      string
 *                      capath:      string
 *                      random_file: string
 *                      egdsocket:   string
 * </pre>
 *
 * The optional third parameter will be filled with some additional information
 * in form of an associative array, if supplied, like the following example:
 * <pre>
 * <?php
 * array (
 *    'effective_url' => 'http://www.example.com/',
 *    'response_code' => 302,
 *    'connect_code' => 0,
 *    'filetime' => -1,
 *    'total_time' => 0.212348,
 *    'namelookup_time' => 0.038296,
 *    'connect_time' => 0.104144,
 *    'pretransfer_time' => 0.104307,
 *    'starttransfer_time' => 0.212077,
 *    'redirect_time' => 0,
 *    'redirect_count' => 0,
 *    'size_upload' => 0,
 *    'size_download' => 218,
 *    'speed_download' => 1026,
 *    'speed_upload' => 0,
 *    'header_size' => 307,
 *    'request_size' => 103,
 *    'ssl_verifyresult' => 0,
 *    'ssl_engines' =>
 *    array (
 *      0 => 'dynamic',
 *      1 => 'cswift',
 *      2 => 'chil',
 *      3 => 'atalla',
 *      4 => 'nuron',
 *      5 => 'ubsec',
 *      6 => 'aep',
 *      7 => 'sureware',
 *      8 => '4758cca',
 *    ),
 *    'content_length_download' => 218,
 *    'content_length_upload' => 0,
 *    'content_type' => 'text/html',
 *    'httpauth_avail' => 0,
 *    'proxyauth_avail' => 0,
 *    'num_connects' => 1,
 *    'os_errno' => 0,
 *    'error' => '',
 *  )
 * ?>
 * </pre>
 * 
 * Returns the HTTP response(s) as string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_get)
{
	zval *options = NULL, *info = NULL;
	char *URL;
	int URL_len;
	http_request request;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a/!z", &URL, &URL_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	RETVAL_FALSE;

	http_request_init_ex(&request, NULL, HTTP_GET, URL);
	if (SUCCESS == http_request_prepare(&request, options?Z_ARRVAL_P(options):NULL)) {
		http_request_exec(&request);
		if (info) {
			http_request_info(&request, Z_ARRVAL_P(info));
		}
		RETVAL_RESPONSE_OR_BODY(request);
	}
	http_request_dtor(&request);
}
/* }}} */

/* {{{ proto string http_head(string url[, array options[, array &info]])
 *
 * Performs an HTTP HEAD request on the supplied url.
 * 
 * See http_get() for a full list of available parameters and options.
 * 
 * Returns the HTTP response as string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_head)
{
	zval *options = NULL, *info = NULL;
	char *URL;
	int URL_len;
	http_request request;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a/!z", &URL, &URL_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	RETVAL_FALSE;

	http_request_init_ex(&request, NULL, HTTP_HEAD, URL);
	if (SUCCESS == http_request_prepare(&request, options?Z_ARRVAL_P(options):NULL)) {
		http_request_exec(&request);
		if (info) {
			http_request_info(&request, Z_ARRVAL_P(info));
		}
		RETVAL_RESPONSE_OR_BODY(request);
	}
	http_request_dtor(&request);
}
/* }}} */

/* {{{ proto string http_post_data(string url, string data[, array options[, array &info]])
 *
 * Performs an HTTP POST request on the supplied url.
 * 
 * Expects a string as second parameter containing the pre-encoded post data.
 * See http_get() for a full list of available parameters and options.
 *  
 * Returns the HTTP response(s) as string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_post_data)
{
	zval *options = NULL, *info = NULL;
	char *URL, *postdata;
	int postdata_len, URL_len;
	http_request_body body;
	http_request request;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a/!z", &URL, &URL_len, &postdata, &postdata_len, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	RETVAL_FALSE;

	http_request_init_ex(&request, NULL, HTTP_POST, URL);
	request.body = http_request_body_init_ex(&body, HTTP_REQUEST_BODY_CSTRING, postdata, postdata_len, 0);
	if (SUCCESS == http_request_prepare(&request, options?Z_ARRVAL_P(options):NULL)) {
		http_request_exec(&request);
		if (info) {
			http_request_info(&request, Z_ARRVAL_P(info));
		}
		RETVAL_RESPONSE_OR_BODY(request);
	}
	http_request_dtor(&request);
}
/* }}} */

/* {{{ proto string http_post_fields(string url, array data[, array files[, array options[, array &info]]])
 *
 * Performs an HTTP POST request on the supplied url.
 * 
 * Expects an associative array as second parameter, which will be
 * www-form-urlencoded. See http_get() for a full list of available options.
 * 
 * Returns the HTTP response(s) as string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_post_fields)
{
	zval *options = NULL, *info = NULL, *fields, *files = NULL;
	char *URL;
	int URL_len;
	http_request_body body;
	http_request request;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|aa/!z", &URL, &URL_len, &fields, &files, &options, &info) != SUCCESS) {
		RETURN_FALSE;
	}

	if (!http_request_body_fill(&body, Z_ARRVAL_P(fields), files ? Z_ARRVAL_P(files) : NULL)) {
		RETURN_FALSE;
	}

	if (info) {
		zval_dtor(info);
		array_init(info);
	}

	RETVAL_FALSE;

	http_request_init_ex(&request, NULL, HTTP_POST, URL);
	request.body = &body;
	if (SUCCESS == http_request_prepare(&request, options?Z_ARRVAL_P(options):NULL)) {
		http_request_exec(&request);
		if (info) {
			http_request_info(&request, Z_ARRVAL_P(info));
		}
		RETVAL_RESPONSE_OR_BODY(request);
	}
	http_request_dtor(&request);
}
/* }}} */

/* {{{ proto string http_put_file(string url, string file[, array options[, array &info]])
 *
 * Performs an HTTP PUT request on the supplied url.
 * 
 * Expects the second parameter to be a string referencing the file to upload.
 * See http_get() for a full list of available options.
 * 
 * Returns the HTTP response(s) as string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_put_file)
{
	char *URL, *file;
	int URL_len, f_len;
	zval *options = NULL, *info = NULL;
	php_stream *stream;
	php_stream_statbuf ssb;
	http_request_body body;
	http_request request;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a/!z", &URL, &URL_len, &file, &f_len, &options, &info)) {
		RETURN_FALSE;
	}

	if (!(stream = php_stream_open_wrapper_ex(file, "rb", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL, HTTP_DEFAULT_STREAM_CONTEXT))) {
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

	RETVAL_FALSE;

	http_request_init_ex(&request, NULL, HTTP_PUT, URL);
	request.body = http_request_body_init_ex(&body, HTTP_REQUEST_BODY_UPLOADFILE, stream, ssb.sb.st_size, 1);
	if (SUCCESS == http_request_prepare(&request, options?Z_ARRVAL_P(options):NULL)) {
		http_request_exec(&request);
		if (info) {
			http_request_info(&request, Z_ARRVAL_P(info));
		}
		RETVAL_RESPONSE_OR_BODY(request);
	}
	http_request_dtor(&request);
}
/* }}} */

/* {{{ proto string http_put_stream(string url, resource stream[, array options[, array &info]])
 *
 * Performs an HTTP PUT request on the supplied url.
 * 
 * Expects the second parameter to be a resource referencing an already 
 * opened stream, from which the data to upload should be read.
 * See http_get() for a full list of available options.
 * 
 * Returns the HTTP response(s) as string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_put_stream)
{
	zval *resource, *options = NULL, *info = NULL;
	char *URL;
	int URL_len;
	php_stream *stream;
	php_stream_statbuf ssb;
	http_request_body body;
	http_request request;

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

	RETVAL_FALSE;

	http_request_init_ex(&request, NULL, HTTP_PUT, URL);
	request.body = http_request_body_init_ex(&body, HTTP_REQUEST_BODY_UPLOADFILE, stream, ssb.sb.st_size, 0);
	if (SUCCESS == http_request_prepare(&request, options?Z_ARRVAL_P(options):NULL)) {
		http_request_exec(&request);
		if (info) {
			http_request_info(&request, Z_ARRVAL_P(info));
		}
		RETVAL_RESPONSE_OR_BODY(request);
	}
	http_request_dtor(&request);
}
/* }}} */

/* {{{ proto string http_put_data(string url, string data[, array options[, array &info]])
 *
 * Performs an HTTP PUT request on the supplied url.
 * 
 * Expects the second parameter to be a string containing the data to upload.
 * See http_get() for a full list of available options.
 * 
 * Returns the HTTP response(s) as string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_put_data)
{
	char *URL, *data;
	int URL_len, data_len;
	zval *options = NULL, *info = NULL;
	http_request_body body;
	http_request request;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|a/!z", &URL, &URL_len, &data, &data_len, &options, &info)) {
		RETURN_FALSE;
	}
	
	if (info) {
		zval_dtor(info);
		array_init(info);
	}
	
	RETVAL_FALSE;
	
	http_request_init_ex(&request, NULL, HTTP_PUT, URL);
	request.body = http_request_body_init_ex(&body, HTTP_REQUEST_BODY_CSTRING, data, data_len, 0);
	if (SUCCESS == http_request_prepare(&request, options?Z_ARRVAL_P(options):NULL)) {
		http_request_exec(&request);
		if (info) {
			http_request_info(&request, Z_ARRVAL_P(info));
		}
		RETVAL_RESPONSE_OR_BODY(request);
	}
	http_request_dtor(&request);
}
/* }}} */

/* {{{ proto string http_request(int method, string url[, string body[, array options[, array &info]]])
 *
 * Performs a custom HTTP request on the supplied url.
 *
 * Expects the first parameter to be an integer specifying the request method to use.
 * Accepts an optional third string parameter containing the raw request body.
 * See http_get() for a full list of available options.
 *
 * Returns the HTTP response(s) as string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_request)
{
	long meth;
	char *URL, *data = NULL;
	int URL_len, data_len = 0;
	zval *options = NULL, *info = NULL;
	http_request_body body;
	http_request request;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls|sa/!z", &meth, &URL, &URL_len, &data, &data_len, &options, &info)) {
		RETURN_FALSE;
	}
	
	if (info) {
		zval_dtor(info);
		array_init(info);
	}
	
	RETVAL_FALSE;
	
	http_request_init_ex(&request, NULL, meth, URL);
	request.body = http_request_body_init_ex(&body, HTTP_REQUEST_BODY_CSTRING, data, data_len, 0);
	if (SUCCESS == http_request_prepare(&request, options?Z_ARRVAL_P(options):NULL)) {
		http_request_exec(&request);
		if (info) {
			http_request_info(&request, Z_ARRVAL_P(info));
		}
		RETVAL_RESPONSE_OR_BODY(request);
	}
	http_request_dtor(&request);
}
/* }}} */

#ifdef HAVE_CURL_GETFORMDATA
static char *file_get_contents(char *file, size_t *len TSRMLS_DC)
{
	php_stream *s = NULL;
	char *buf = NULL;
	
	if ((s = php_stream_open_wrapper_ex(file, "rb", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL, HTTP_DEFAULT_STREAM_CONTEXT))) {
		*len = php_stream_copy_to_mem(s, &buf, (size_t) -1, 0);
		php_stream_close(s);
	} else {
		*len = 0;
	}
	return buf;
}
struct FormData {
	struct FormData *next;
	int type;
	char *line;
	size_t length;
};
CURLcode Curl_getFormData(struct FormData **, struct curl_httppost *post, curl_off_t *size);

/* {{{ proto string http_request_body_encode(array fields, array files)
 *
 * Generate x-www-form-urlencoded resp. form-data encoded request body.
 *
 * Returns encoded string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_request_body_encode)
{
	zval *fields = NULL, *files = NULL;
	HashTable *fields_ht, *files_ht;
	http_request_body body;
	phpstr rbuf;
	struct FormData *data, *ptr;
	curl_off_t size;
	char *fdata = NULL;
	size_t fsize = 0;
	CURLcode rc;
	int fgc_error = 0;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a!a!", &fields, &files)) {
		RETURN_FALSE;
	}
	
	fields_ht = (fields && Z_TYPE_P(fields) == IS_ARRAY) ? Z_ARRVAL_P(fields) : NULL;
	files_ht = (files && Z_TYPE_P(files) == IS_ARRAY) ? Z_ARRVAL_P(files) : NULL;
	if (!http_request_body_fill(&body, fields_ht, files_ht)) {
		RETURN_FALSE;
	}
	
	switch (body.type) {
		case HTTP_REQUEST_BODY_CURLPOST:
			if (CURLE_OK != (rc = Curl_getFormData(&data, body.data, &size))) {
				http_error_ex(HE_WARNING, HTTP_E_RUNTIME, "Could not encode request body: %s", curl_easy_strerror(rc));
				RETVAL_FALSE;
			} else {
				phpstr_init_ex(&rbuf, (size_t) size, PHPSTR_INIT_PREALLOC);
				for (ptr = data; ptr; ptr = ptr->next) {
					if (!fgc_error) {
						if (ptr->type) {
							if ((fdata = file_get_contents(ptr->line, &fsize TSRMLS_CC))) {
								phpstr_append(&rbuf, fdata, fsize);
								efree(fdata);
							} else {
								fgc_error = 1;
							}
						} else {
							phpstr_append(&rbuf, ptr->line, ptr->length);
						}
					}
					curl_free(ptr->line);
				}
				curl_free(data);
				if (fgc_error) {
					phpstr_dtor(&rbuf);
					RETVAL_FALSE;
				} else {
					RETVAL_PHPSTR_VAL(&rbuf);
				}
			}
			http_request_body_dtor(&body);
			break;
		
		case HTTP_REQUEST_BODY_CSTRING:
			RETVAL_STRINGL(body.data, body.size, 0);
			break;
		
		default:
			http_request_body_dtor(&body);
			RETVAL_FALSE;
			break;
	}
}
#endif /* HAVE_CURL_GETFORMDATA */
#endif /* HTTP_HAVE_CURL */
/* }}} HAVE_CURL */

/* {{{ proto int http_request_method_register(string method)
 *
 * Register a custom request method.
 * 
 * Expects a string parameter containing the request method name to register.
 * 
 * Returns the ID of the request method on success, or FALSE on failure.
 */
PHP_FUNCTION(http_request_method_register)
{
	char *method;
	int method_len;
	ulong existing;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &method, &method_len)) {
		RETURN_FALSE;
	}
	if ((existing = http_request_method_exists(1, 0, method))) {
		RETURN_LONG((long) existing);
	}

	RETVAL_LONG((long) http_request_method_register(method, method_len));
}
/* }}} */

/* {{{ proto bool http_request_method_unregister(mixed method)
 *
 * Unregister a previously registered custom request method.
 * 
 * Expects either the request method name or ID.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_FUNCTION(http_request_method_unregister)
{
	zval *method;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &method)) {
		RETURN_FALSE;
	}

	switch (Z_TYPE_P(method)) {
		case IS_OBJECT:
			convert_to_string(method);
		case IS_STRING:
			if (is_numeric_string(Z_STRVAL_P(method), Z_STRLEN_P(method), NULL, NULL, 1)) {
				convert_to_long(method);
			} else {
				int mn;
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

/* {{{ proto int http_request_method_exists(mixed method)
 *
 * Check if a request method is registered (or available by default).
 * 
 * Expects either the request method name or ID as parameter.
 * 
 * Returns TRUE if the request method is known, else FALSE.
 */
PHP_FUNCTION(http_request_method_exists)
{
	if (return_value_used) {
		zval *method;

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &method)) {
			RETURN_FALSE;
		}

		switch (Z_TYPE_P(method)) {
			case IS_OBJECT:
				convert_to_string(method);
			case IS_STRING:
				if (is_numeric_string(Z_STRVAL_P(method), Z_STRLEN_P(method), NULL, NULL, 1)) {
					convert_to_long(method);
				} else {
					RETURN_LONG((long) http_request_method_exists(1, 0, Z_STRVAL_P(method)));
				}
			case IS_LONG:
				RETURN_LONG((long) http_request_method_exists(0, (int) Z_LVAL_P(method), NULL));
			default:
				RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto string http_request_method_name(int method)
 *
 * Get the literal string representation of a standard or registered request method.
 * 
 * Expects the request method ID as parameter.
 * 
 * Returns the request method name as string on success, or FALSE on failure.
 */
PHP_FUNCTION(http_request_method_name)
{
	if (return_value_used) {
		long method;

		if ((SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &method)) || (method < 0)) {
			RETURN_FALSE;
		}

		RETURN_STRING(estrdup(http_request_method_name((int) method)), 0);
	}
}
/* }}} */

/* {{{ */
#ifdef HTTP_HAVE_ZLIB

/* {{{  proto string http_deflate(string data[, int flags = 0])
 *
 * Compress data with gzip, zlib AKA deflate or raw deflate encoding.
 * 
 * Expects the first parameter to be a string containing the data that should
 * be encoded.
 * 
 * Returns the encoded string on success, or NULL on failure.
 */
PHP_FUNCTION(http_deflate)
{
	char *data;
	int data_len;
	long flags = 0;
	
	RETVAL_NULL();
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &data, &data_len, &flags)) {
		char *encoded;
		size_t encoded_len;
		
		if (SUCCESS == http_encoding_deflate(flags, data, data_len, &encoded, &encoded_len)) {
			RETURN_STRINGL(encoded, (int) encoded_len, 0);
		}
	}
}
/* }}} */

/* {{{ proto string http_inflate(string data)
 *
 * Decompress data compressed with either gzip, deflate AKA zlib or raw
 * deflate encoding.
 * 
 * Expects a string as parameter containing the compressed data.
 * 
 * Returns the decoded string on success, or NULL on failure.
 */
PHP_FUNCTION(http_inflate)
{
	char *data;
	int data_len;
	
	RETVAL_NULL();
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &data, &data_len)) {
		char *decoded;
		size_t decoded_len;
		
		if (SUCCESS == http_encoding_inflate(data, data_len, &decoded, &decoded_len)) {
			RETURN_STRINGL(decoded, (int) decoded_len, 0);
		}
	}
}
/* }}} */

/* {{{ proto string ob_deflatehandler(string data, int mode)
 *
 * For use with ob_start(). The deflate output buffer handler can only be used once.
 * It conflicts with ob_gzhandler and zlib.output_compression as well and should
 * not be used after ext/mbstrings mb_output_handler and ext/sessions URL-Rewriter (AKA
 * session.use_trans_sid).
 */
PHP_FUNCTION(ob_deflatehandler)
{
	char *data;
	int data_len;
	long mode;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &data, &data_len, &mode)) {
		RETURN_FALSE;
	}

	http_ob_deflatehandler(data, data_len, &Z_STRVAL_P(return_value), (uint *) &Z_STRLEN_P(return_value), mode);
	Z_TYPE_P(return_value) = Z_STRVAL_P(return_value) ? IS_STRING : IS_NULL;
}
/* }}} */

/* {{{ proto string ob_inflatehandler(string data, int mode)
 *
 * For use with ob_start().  Same restrictions as with ob_deflatehandler apply.
 */
PHP_FUNCTION(ob_inflatehandler)
{
	char *data;
	int data_len;
	long mode;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &data, &data_len, &mode)) {
		RETURN_FALSE;
	}
	
	http_ob_inflatehandler(data, data_len, &Z_STRVAL_P(return_value), (uint *) &Z_STRLEN_P(return_value), mode);
	Z_TYPE_P(return_value) = Z_STRVAL_P(return_value) ? IS_STRING : IS_NULL;
}
/* }}} */

#endif /* HTTP_HAVE_ZLIB */
/* }}} */

/* {{{ proto int http_support([int feature = 0])
 *
 * Check for feature that require external libraries.
 * 
 * Accepts an optional in parameter specifying which feature to probe for.
 * If the parameter is 0 or omitted, the return value contains a bitmask of 
 * all supported features that depend on external libraries.
 * 
 * Available features to probe for are:
 * <ul> 
 *  <li> HTTP_SUPPORT: always set
 *  <li> HTTP_SUPPORT_REQUESTS: whether ext/http was linked against libcurl,
 *       and HTTP requests can be issued
 *  <li> HTTP_SUPPORT_SSLREQUESTS: whether libcurl was linked against openssl,
 *       and SSL requests can be issued 
 *  <li> HTTP_SUPPORT_ENCODINGS: whether ext/http was linked against zlib,
 *       and compressed HTTP responses can be decoded
 *  <li> HTTP_SUPPORT_MAGICMIME: whether ext/http was linked against libmagic,
 *       and the HttpResponse::guessContentType() method is usable
 * </ul>
 * 
 * Returns int, whether requested feature is supported, or a bitmask with
 * all supported features.
 */
PHP_FUNCTION(http_support)
{
	long feature = 0;
	
	RETVAL_LONG(0L);
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &feature)) {
		RETVAL_LONG(http_support(feature));
	}
}
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

