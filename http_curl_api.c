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

#ifdef PHP_WIN32
#	include <winsock2.h>
#endif

#include <curl/curl.h>

#include "php.h"
#include "php_http.h"
#include "php_http_api.h"
#include "php_http_curl_api.h"
#include "php_http_std_defs.h"

#ifdef ZEND_ENGINE_2
#	include "ext/standard/php_http.h"
#endif

#include "phpstr/phpstr.h"

ZEND_DECLARE_MODULE_GLOBALS(http)

#if LIBCURL_VERSION_NUM >= 0x070c01
#	define http_curl_reset(ch) curl_easy_reset(ch)
#else
#	define http_curl_reset(ch)
#endif

#if LIBCURL_VERSION_NUM < 0x070c00
#	define curl_easy_strerror(code) "unkown error"
#endif

#define http_curl_startup(ch, clean_curl, URL, options) \
	if (!ch) { \
		if (!(ch = curl_easy_init())) { \
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not initialize curl"); \
			return FAILURE; \
		} \
		clean_curl = 1; \
	} else { \
		http_curl_reset(ch); \
	} \
	http_curl_setopts(ch, URL, options);

#define http_curl_perform(ch, clean_curl) \
	{ \
		CURLcode result; \
		if (CURLE_OK != (result = curl_easy_perform(ch))) { \
			http_curl_cleanup(ch, clean_curl); \
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not perform request: %s", curl_easy_strerror(result)); \
			return FAILURE; \
		} \
	}

#define http_curl_cleanup(ch, clean_curl) \
	phpstr_free(&HTTP_G(curlbuf)); \
	zend_llist_clean(&HTTP_G(to_free)); \
	if (clean_curl) { \
		curl_easy_cleanup(ch); \
		ch = NULL; \
	}

#define http_curl_copybuf(d, l) \
	phpstr_data(&HTTP_G(curlbuf), d, l)

#define http_curl_copystr(s) _http_curl_copystr((s) TSRMLS_CC)
static inline char *_http_curl_copystr(const char *str TSRMLS_DC);

#define http_curl_setopts(c, u, o) _http_curl_setopts((c), (u), (o) TSRMLS_CC)
static void _http_curl_setopts(CURL *ch, const char *url, HashTable *options TSRMLS_DC);

#define http_curl_getopt(o, k) _http_curl_getopt((o), (k) TSRMLS_CC, 0)
#define http_curl_getopt1(o, k, t1) _http_curl_getopt((o), (k) TSRMLS_CC, 1, (t1))
#define http_curl_getopt2(o, k, t1, t2) _http_curl_getopt((o), (k) TSRMLS_CC, 2, (t1), (t2))
static inline zval *_http_curl_getopt(HashTable *options, char *key TSRMLS_DC, int checks, ...);

static size_t http_curl_body_callback(char *, size_t, size_t, void *);
static size_t http_curl_hdrs_callback(char *, size_t, size_t, void *);

#define http_curl_getinfo(c, h) _http_curl_getinfo((c), (h) TSRMLS_CC)
static inline void _http_curl_getinfo(CURL *ch, HashTable *info TSRMLS_DC);

/* {{{ static inline char *http_curl_copystr(char *) */
static inline char *_http_curl_copystr(const char *str TSRMLS_DC)
{
	char *new_str = estrdup(str);
	zend_llist_add_element(&HTTP_G(to_free), &new_str);
	return new_str;
}
/* }}} */

/* {{{ static size_t http_curl_body_callback(char *, size_t, size_t, void *) */
static size_t http_curl_body_callback(char *buf, size_t len, size_t n, void *s)
{
	TSRMLS_FETCH();

	phpstr_append(&HTTP_G(curlbuf), buf, len *= n);
	return len;
}
/* }}} */

/* {{{ static size_t http_curl_hdrs_callback(char *, size_t, size_t, void *) */
static size_t http_curl_hdrs_callback(char *buf, size_t len, size_t n, void *s)
{
	TSRMLS_FETCH();

	/* discard previous headers */
	if (HTTP_G(curlbuf).used && (!strncmp(buf, "HTTP/1.", sizeof("HTTP/1.") - 1))) {
		phpstr_free(&HTTP_G(curlbuf));
	}

	phpstr_append(&HTTP_G(curlbuf), buf, len *= n);
	return len;
}
/* }}} */

/* {{{ static inline zval *http_curl_getopt(HashTable *, char *, int, ...) */
static inline zval *_http_curl_getopt(HashTable *options, char *key TSRMLS_DC, int checks, ...)
{
	zval **zoption;
	va_list types;
	int i;

	if (SUCCESS != zend_hash_find(options, key, strlen(key) + 1, (void **) &zoption)) {
		return NULL;
	}
	if (checks < 1) {
		return *zoption;
	}

	va_start(types, checks);
	for (i = 0; i < checks; ++i) {
		if ((va_arg(types, int)) == (Z_TYPE_PP(zoption))) {
			va_end(types);
			return *zoption;
		}
	}
	va_end(types);
	return NULL;
}
/* }}} */

/* {{{ static void http_curl_setopts(CURL *, char *, HashTable *) */
static void _http_curl_setopts(CURL *ch, const char *url, HashTable *options TSRMLS_DC)
{
	zval *zoption;
	zend_bool range_req = 0;

	/* standard options */
	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_HEADER, 0);
	curl_easy_setopt(ch, CURLOPT_FILETIME, 1);
	curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(ch, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, http_curl_body_callback);
	curl_easy_setopt(ch, CURLOPT_HEADERFUNCTION, http_curl_hdrs_callback);
#if defined(ZTS) && (LIBCURL_VERSION_NUM >= 0x070a00)
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
#endif

	if ((!options) || (1 > zend_hash_num_elements(options))) {
		return;
	}

	/* proxy */
	if (zoption = http_curl_getopt1(options, "proxyhost", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_PROXY, http_curl_copystr(Z_STRVAL_P(zoption)));
		/* port */
		if (zoption = http_curl_getopt1(options, "proxyport", IS_LONG)) {
			curl_easy_setopt(ch, CURLOPT_PROXYPORT, Z_LVAL_P(zoption));
		}
		/* user:pass */
		if (zoption = http_curl_getopt1(options, "proxyauth", IS_STRING)) {
			curl_easy_setopt(ch, CURLOPT_PROXYUSERPWD, http_curl_copystr(Z_STRVAL_P(zoption)));
		}
#if LIBCURL_VERSION_NUM >= 0x070a07
		/* auth method */
		if (zoption = http_curl_getopt1(options, "proxyauthtype", IS_LONG)) {
			curl_easy_setopt(ch, CURLOPT_PROXYAUTH, Z_LVAL_P(zoption));
		}
#endif
	}

	/* outgoing interface */
	if (zoption = http_curl_getopt1(options, "interface", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_INTERFACE, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* another port */
	if (zoption = http_curl_getopt1(options, "port", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_PORT, Z_LVAL_P(zoption));
	}

	/* auth */
	if (zoption = http_curl_getopt1(options, "httpauth", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_USERPWD, http_curl_copystr(Z_STRVAL_P(zoption)));
	}
#if LIBCURL_VERSION_NUM >= 0x070a06
	if (zoption = http_curl_getopt1(options, "httpauthtype", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_HTTPAUTH, Z_LVAL_P(zoption));
	}
#endif

	/* compress, empty string enables deflate and gzip */
	if (zoption = http_curl_getopt2(options, "compress", IS_LONG, IS_BOOL)) {
		if (Z_LVAL_P(zoption)) {
			curl_easy_setopt(ch, CURLOPT_ENCODING, "");
		}
	}

	/* redirects, defaults to 0 */
	if (zoption = http_curl_getopt1(options, "redirect", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, Z_LVAL_P(zoption) ? 1 : 0);
		curl_easy_setopt(ch, CURLOPT_MAXREDIRS, Z_LVAL_P(zoption));
		if (zoption = http_curl_getopt2(options, "unrestrictedauth", IS_LONG, IS_BOOL)) {
			curl_easy_setopt(ch, CURLOPT_UNRESTRICTED_AUTH, Z_LVAL_P(zoption));
		}
	} else {
		curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 0);
	}

	/* referer */
	if (zoption = http_curl_getopt1(options, "referer", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_REFERER, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if (zoption = http_curl_getopt1(options, "useragent", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_USERAGENT, http_curl_copystr(Z_STRVAL_P(zoption)));
	} else {
		curl_easy_setopt(ch, CURLOPT_USERAGENT,
			"PECL::HTTP/" HTTP_PEXT_VERSION " (PHP/" PHP_VERSION ")");
	}

	/* additional headers, array('name' => 'value') */
	if (zoption = http_curl_getopt1(options, "headers", IS_ARRAY)) {
		char *header_key;
		long header_idx;
		struct curl_slist *headers = NULL;

		FOREACH_KEY(zoption, header_key, header_idx) {
			if (header_key) {
				zval **header_val;
				if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void **) &header_val)) {
					char header[1024] = {0};
					snprintf(header, 1023, "%s: %s", header_key, Z_STRVAL_PP(header_val));
					headers = curl_slist_append(headers, http_curl_copystr(header));
				}

				/* reset */
				header_key = NULL;
			}
		}

		if (headers) {
			curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
		}
	}

	/* cookies, array('name' => 'value') */
	if (zoption = http_curl_getopt1(options, "cookies", IS_ARRAY)) {
		char *cookie_key = NULL;
		long cookie_idx = 0;
		phpstr *qstr = phpstr_new();

		FOREACH_KEY(zoption, cookie_key, cookie_idx) {
			if (cookie_key) {
				zval **cookie_val;
				if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void **) &cookie_val)) {
					phpstr_appendf(qstr, "%s=%s; ", cookie_key, Z_STRVAL_PP(cookie_val));
				}

				/* reset */
				cookie_key = NULL;
			}
		}

		if (qstr->used) {
			phpstr_fix(qstr);
			curl_easy_setopt(ch, CURLOPT_COOKIE, http_curl_copystr(qstr->data));
		}
		phpstr_dtor(qstr);
	}

	/* cookiestore */
	if (zoption = http_curl_getopt1(options, "cookiestore", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_COOKIEFILE, http_curl_copystr(Z_STRVAL_P(zoption)));
		curl_easy_setopt(ch, CURLOPT_COOKIEJAR, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* resume */
	if (zoption = http_curl_getopt1(options, "resume", IS_LONG)) {
		range_req = 1;
		curl_easy_setopt(ch, CURLOPT_RESUME_FROM, Z_LVAL_P(zoption));
	}

	/* maxfilesize */
	if (zoption = http_curl_getopt1(options, "maxfilesize", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_MAXFILESIZE, Z_LVAL_P(zoption));
	}

	/* lastmodified */
	if (zoption = http_curl_getopt1(options, "lastmodified", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_TIMECONDITION, range_req ? CURL_TIMECOND_IFUNMODSINCE : CURL_TIMECOND_IFMODSINCE);
		curl_easy_setopt(ch, CURLOPT_TIMEVALUE, Z_LVAL_P(zoption));
	}

	/* timeout */
	if (zoption = http_curl_getopt1(options, "timeout", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_TIMEOUT, Z_LVAL_P(zoption));
	}

	/* connecttimeout */
	if (zoption = http_curl_getopt1(options, "connecttimeout", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, Z_LVAL_P(zoption));
	}
}
/* }}} */

/* {{{ static inline http_curl_getinfo(CURL, HashTable *) */
static inline void _http_curl_getinfo(CURL *ch, HashTable *info TSRMLS_DC)
{
	zval array;
	Z_ARRVAL(array) = info;

#define HTTP_CURL_INFO(I) HTTP_CURL_INFO_EX(I, I)
#define HTTP_CURL_INFO_EX(I, X) \
	switch (CURLINFO_ ##I & ~CURLINFO_MASK) \
	{ \
		case CURLINFO_STRING: \
		{ \
			char *c; \
			if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_ ##I, &c)) { \
				add_assoc_string(&array, pretty_key(http_curl_copystr(#X), sizeof(#X)-1, 0, 0), c ? c : "", 1); \
			} \
		} \
		break; \
\
		case CURLINFO_DOUBLE: \
		{ \
			double d; \
			if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_ ##I, &d)) { \
				add_assoc_double(&array, pretty_key(http_curl_copystr(#X), sizeof(#X)-1, 0, 0), d); \
			} \
		} \
		break; \
\
		case CURLINFO_LONG: \
		{ \
			long l; \
			if (CURLE_OK == curl_easy_getinfo(ch, CURLINFO_ ##I, &l)) { \
				add_assoc_long(&array, pretty_key(http_curl_copystr(#X), sizeof(#X)-1, 0, 0), l); \
			} \
		} \
		break; \
	}

	HTTP_CURL_INFO(EFFECTIVE_URL);

#if LIBCURL_VERSION_NUM >= 0x070a07
	HTTP_CURL_INFO(RESPONSE_CODE);
#else
	HTTP_CURL_INFO_EX(HTTP_CODE, RESPONSE_CODE);
#endif
	HTTP_CURL_INFO(HTTP_CONNECTCODE);

#if LIBCURL_VERSION_NUM >= 0x070500
	HTTP_CURL_INFO(FILETIME);
#endif
	HTTP_CURL_INFO(TOTAL_TIME);
	HTTP_CURL_INFO(NAMELOOKUP_TIME);
	HTTP_CURL_INFO(CONNECT_TIME);
	HTTP_CURL_INFO(PRETRANSFER_TIME);
	HTTP_CURL_INFO(STARTTRANSFER_TIME);
#if LIBCURL_VERSION_NUM >= 0x070907
	HTTP_CURL_INFO(REDIRECT_TIME);
	HTTP_CURL_INFO(REDIRECT_COUNT);
#endif

	HTTP_CURL_INFO(SIZE_UPLOAD);
	HTTP_CURL_INFO(SIZE_DOWNLOAD);
	HTTP_CURL_INFO(SPEED_DOWNLOAD);
	HTTP_CURL_INFO(SPEED_UPLOAD);

	HTTP_CURL_INFO(HEADER_SIZE);
	HTTP_CURL_INFO(REQUEST_SIZE);

	HTTP_CURL_INFO(SSL_VERIFYRESULT);
#if LIBCURL_VERSION_NUM >= 0x070c03
	/*HTTP_CURL_INFO(SSL_ENGINES);
		todo: CURLINFO_SLIST */
#endif

	HTTP_CURL_INFO(CONTENT_LENGTH_DOWNLOAD);
	HTTP_CURL_INFO(CONTENT_LENGTH_UPLOAD);
	HTTP_CURL_INFO(CONTENT_TYPE);

#if LIBCURL_VERSION_NUM >= 0x070a03
	/*HTTP_CURL_INFO(PRIVATE);*/
#endif

#if LIBCURL_VERSION_NUM >= 0x070a08
	HTTP_CURL_INFO(HTTPAUTH_AVAIL);
	HTTP_CURL_INFO(PROXYAUTH_AVAIL);
#endif

#if LIBCURL_VERSION_NUM >= 0x070c02
	/*HTTP_CURL_INFO(OS_ERRNO);*/
#endif

#if LIBCURL_VERSION_NUM >= 0x070c03
	HTTP_CURL_INFO(NUM_CONNECTS);
#endif
}
/* }}} */

/* {{{ STATUS http_get_ex(CURL *, char *, HashTable *, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_get_ex(CURL *ch, const char *URL, HashTable *options,
	HashTable *info, char **data, size_t *data_len TSRMLS_DC)
{
	zend_bool clean_curl = 0;

	http_curl_startup(ch, clean_curl, URL, options);
	curl_easy_setopt(ch, CURLOPT_HTTPGET, 1);
	http_curl_perform(ch, clean_curl);

	if (info) {
		http_curl_getinfo(ch, info);
	}

	http_curl_copybuf(data, data_len);
	http_curl_cleanup(ch, clean_curl);

	return SUCCESS;
}

/* {{{ STATUS http_head_ex(CURL *, char *, HashTable *, HashTable *, char **data, size_t *) */
PHP_HTTP_API STATUS _http_head_ex(CURL *ch, const char *URL, HashTable *options,
	HashTable *info, char **data, size_t *data_len TSRMLS_DC)
{
	zend_bool clean_curl = 0;

	http_curl_startup(ch, clean_curl, URL, options);
	curl_easy_setopt(ch, CURLOPT_NOBODY, 1);
	http_curl_perform(ch, clean_curl);

	if (info) {
		http_curl_getinfo(ch, info);
	}

	http_curl_copybuf(data, data_len);
	http_curl_cleanup(ch, clean_curl);

	return SUCCESS;
}

/* {{{ STATUS http_post_data_ex(CURL *, char *, char *, size_t, HashTable *, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_post_data_ex(CURL *ch, const char *URL, char *postdata,
	size_t postdata_len, HashTable *options, HashTable *info, char **data,
	size_t *data_len TSRMLS_DC)
{
	zend_bool clean_curl = 0;

	http_curl_startup(ch, clean_curl, URL, options);
	curl_easy_setopt(ch, CURLOPT_POST, 1);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, postdata);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, postdata_len);
	http_curl_perform(ch, clean_curl);

	if (info) {
		http_curl_getinfo(ch, info);
	}

	http_curl_copybuf(data, data_len);
	http_curl_cleanup(ch, clean_curl);

	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_post_array_ex(CURL *, char *, HashTable *, HashTable *, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_post_array_ex(CURL *ch, const char *URL, HashTable *postarray,
	HashTable *options, HashTable *info, char **data, size_t *data_len TSRMLS_DC)
{
	STATUS status;
	char *encoded;
	size_t encoded_len;

	if (SUCCESS != http_urlencode_hash_ex(postarray, 1, NULL, 0, &encoded, &encoded_len)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not encode post data");
		return FAILURE;
	}

	status = http_post_data_ex(ch, URL, encoded, encoded_len, options, info, data, data_len);
	efree(encoded);
	
	return status;
}
/* }}} */

/* {{{ STATUS http_post_curldata_ex(CURL *, char *, curl_httppost *, HashTable *, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_post_curldata_ex(CURL *ch, const char *URL,
	struct curl_httppost *curldata,	HashTable *options, HashTable *info,
	char **data, size_t *data_len TSRMLS_DC)
{
	zend_bool clean_curl = 0;

	http_curl_startup(ch, clean_curl, URL, options);
	curl_easy_setopt(ch, CURLOPT_POST, 1);
	curl_easy_setopt(ch, CURLOPT_HTTPPOST, curldata);
	http_curl_perform(ch, clean_curl);

	if (info) {
		http_curl_getinfo(ch, info);
	}

	http_curl_copybuf(data, data_len);
	http_curl_cleanup(ch, clean_curl);

	return SUCCESS;
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

