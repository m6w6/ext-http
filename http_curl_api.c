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
#	define _WINSOCKAPI_
#	define ZEND_INCLUDE_FULL_WINDOWS_HEADERS
#	include <winsock2.h>
#	include <sys/types.h>
#endif

#include <curl/curl.h>
#include <curl/easy.h>

#include "php.h"
#include "php_http.h"
#include "php_http_api.h"
#include "php_http_curl_api.h"

#include "ext/standard/php_smart_str.h"

ZEND_DECLARE_MODULE_GLOBALS(http)

#define http_curl_startup(ch, clean_curl, URL, options) \
	if (!ch) { \
		if (!(ch = curl_easy_init())) { \
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not initialize curl"); \
			return FAILURE; \
		} \
		clean_curl = 1; \
	} \
	http_curl_initbuf(); \
	http_curl_setopts(ch, URL, options);


#define http_curl_cleanup(ch, clean_curl) \
	http_curl_freestr(); \
	http_curl_freebuf(); \
	if (clean_curl) { \
		curl_easy_cleanup(ch); \
		ch = NULL; \
	}

#define http_curl_freestr() \
	zend_llist_clean(&HTTP_G(to_free))

#define http_curl_initbuf() http_curl_initbuf_ex(0)

#define http_curl_initbuf_ex(chunk_size) \
	{ \
		size_t size = (chunk_size > 0) ? chunk_size : HTTP_CURLBUF_SIZE; \
		http_curl_freebuf(); \
		HTTP_G(curlbuf).data = emalloc(size); \
		HTTP_G(curlbuf).free = size; \
		HTTP_G(curlbuf).size = size; \
	}

#define http_curl_freebuf() \
	if (HTTP_G(curlbuf).data) { \
		efree(HTTP_G(curlbuf).data); \
		HTTP_G(curlbuf).data = NULL; \
	} \
	HTTP_G(curlbuf).used = 0; \
	HTTP_G(curlbuf).free = 0; \
	HTTP_G(curlbuf).size = 0;

#define http_curl_copybuf(data, size) \
	* size = HTTP_G(curlbuf).used; \
	* data = ecalloc(1, HTTP_G(curlbuf).used + 1); \
	memcpy(* data, HTTP_G(curlbuf).data, * size);

#define http_curl_sizebuf(for_size) \
	{ \
		size_t size = (for_size); \
		if (size > HTTP_G(curlbuf).free) { \
			size_t bsize = HTTP_G(curlbuf).size; \
			while (size > bsize) { \
				bsize *= 2; \
			} \
			HTTP_G(curlbuf).data = erealloc(HTTP_G(curlbuf).data, HTTP_G(curlbuf).used + bsize); \
			HTTP_G(curlbuf).free += bsize; \
		} \
	}


#define http_curl_copystr(s) _http_curl_copystr((s) TSRMLS_CC)
static inline char *_http_curl_copystr(const char *str TSRMLS_DC);

#define http_curl_setopts(c, u, o) _http_curl_setopts((c), (u), (o) TSRMLS_CC)
static inline void _http_curl_setopts(CURL *ch, const char *url, HashTable *options TSRMLS_DC);

#define http_curl_getopt(o, k) _http_curl_getopt((o), (k) TSRMLS_CC, 0)
#define http_curl_getopt1(o, k, t1) _http_curl_getopt((o), (k) TSRMLS_CC, 1, (t1))
#define http_curl_getopt2(o, k, t1, t2) _http_curl_getopt((o), (k) TSRMLS_CC, 2, (t1), (t2))
static inline zval *_http_curl_getopt(HashTable *options, char *key TSRMLS_DC, int checks, ...);

static size_t http_curl_body_callback(char *, size_t, size_t, void *);
static size_t http_curl_hdrs_callback(char *, size_t, size_t, void *);

#define http_curl_getinfo(c, h) _http_curl_getinfo((c), (h) TSRMLS_CC)
static inline void _http_curl_getinfo(CURL *ch, HashTable *info TSRMLS_DC);
#define http_curl_getinfo_ex(c, i, a) _http_curl_getinfo_ex((c), (i), (a) TSRMLS_CC)
static inline void _http_curl_getinfo_ex(CURL *ch, CURLINFO i, zval *array TSRMLS_DC);
#define http_curl_getinfoname(i) _http_curl_getinfoname((i) TSRMLS_CC)
static inline char *_http_curl_getinfoname(CURLINFO i TSRMLS_DC);

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

	http_curl_sizebuf(len *= n);

	memcpy(HTTP_G(curlbuf).data + HTTP_G(curlbuf).used, buf, len);
	HTTP_G(curlbuf).free -= len;
	HTTP_G(curlbuf).used += len;
	return len;
}
/* }}} */

/* {{{ static size_t http_curl_hdrs_callback(char *, size_t, size_t, void *) */
static size_t http_curl_hdrs_callback(char *buf, size_t len, size_t n, void *s)
{
	TSRMLS_FETCH();

	/* discard previous headers */
	if ((HTTP_G(curlbuf).used) && (!strncmp(buf, "HTTP/1.", sizeof("HTTP/1.") - 1))) {
		http_curl_initbuf();
	}
	http_curl_sizebuf(len *= n);

	memcpy(HTTP_G(curlbuf).data + HTTP_G(curlbuf).used, buf, len);
	HTTP_G(curlbuf).free -= len;
	HTTP_G(curlbuf).used += len;
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

/* {{{ static inline void http_curl_setopts(CURL *, char *, HashTable *) */
static inline void _http_curl_setopts(CURL *ch, const char *url, HashTable *options TSRMLS_DC)
{
	zval *zoption;

	/* standard options */
	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_HEADER, 0);
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
#if LIBCURL_VERSION_NUM > 0x070a06
		/* auth method */
		if (zoption = http_curl_getopt1(options, "proxyauthtype", IS_LONG)) {
			curl_easy_setopt(ch, CURLOPT_PROXYAUTH, Z_LVAL_P(zoption));
		}
#endif
	}

	/* auth */
	if (zoption = http_curl_getopt1(options, "httpauth", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_USERPWD, http_curl_copystr(Z_STRVAL_P(zoption)));
	}
#if LIBCURL_VERSION_NUM > 0x070a05
	if (zoption = http_curl_getopt1(options, "httpauthtype", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_HTTPAUTH, Z_LVAL_P(zoption));
	}
#endif

	/* compress, enabled by default (empty string enables deflate and gzip) */
	if (zoption = http_curl_getopt2(options, "compress", IS_LONG, IS_BOOL)) {
		if (Z_LVAL_P(zoption)) {
			curl_easy_setopt(ch, CURLOPT_ENCODING, "");
		}
	}

	/* another port */
	if (zoption = http_curl_getopt1(options, "port", IS_LONG)) {
		curl_easy_setopt(ch, CURLOPT_PORT, Z_LVAL_P(zoption));
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
			"PECL::HTTP/" PHP_EXT_HTTP_VERSION " (PHP/" PHP_VERSION ")");
	}

	/* cookies, array('name' => 'value') */
	if (zoption = http_curl_getopt1(options, "cookies", IS_ARRAY)) {
		char *cookie_key;
		zval **cookie_val;
		int key_type;
		smart_str qstr = {0};

		zend_hash_internal_pointer_reset(Z_ARRVAL_P(zoption));
		while (HASH_KEY_NON_EXISTANT != (key_type = zend_hash_get_current_key_type(Z_ARRVAL_P(zoption)))) {
			if (key_type == HASH_KEY_IS_STRING) {
				zend_hash_get_current_key(Z_ARRVAL_P(zoption), &cookie_key, NULL, 0);
				zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void **) &cookie_val);
				smart_str_appends(&qstr, cookie_key);
				smart_str_appendl(&qstr, "=", 1);
				smart_str_appendl(&qstr, Z_STRVAL_PP(cookie_val), Z_STRLEN_PP(cookie_val));
				smart_str_appendl(&qstr, "; ", 2);
				zend_hash_move_forward(Z_ARRVAL_P(zoption));
			}
		}
		smart_str_0(&qstr);

		if (qstr.c) {
			curl_easy_setopt(ch, CURLOPT_COOKIE, http_curl_copystr(qstr.c));
			efree(qstr.c);
		}
	}

	/* cookiestore */
	if (zoption = http_curl_getopt1(options, "cookiestore", IS_STRING)) {
		curl_easy_setopt(ch, CURLOPT_COOKIEFILE, http_curl_copystr(Z_STRVAL_P(zoption)));
		curl_easy_setopt(ch, CURLOPT_COOKIEJAR, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* additional headers, array('name' => 'value') */
	if (zoption = http_curl_getopt1(options, "headers", IS_ARRAY)) {
		int key_type;
		char *header_key, header[1024] = {0};
		zval **header_val;
		struct curl_slist *headers = NULL;

		zend_hash_internal_pointer_reset(Z_ARRVAL_P(zoption));
		while (HASH_KEY_NON_EXISTANT != (key_type = zend_hash_get_current_key_type(Z_ARRVAL_P(zoption)))) {
			if (key_type == HASH_KEY_IS_STRING) {
				zend_hash_get_current_key(Z_ARRVAL_P(zoption), &header_key, NULL, 0);
				zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void **) &header_val);
				snprintf(header, 1023, "%s: %s", header_key, Z_STRVAL_PP(header_val));
				headers = curl_slist_append(headers, http_curl_copystr(header));
				zend_hash_move_forward(Z_ARRVAL_P(zoption));
			}
		}
		if (headers) {
			curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
		}
	}
}
/* }}} */

/* {{{ static inline char *http_curl_getinfoname(CURLINFO) */
static inline char *_http_curl_getinfoname(CURLINFO i TSRMLS_DC)
{
#define CASE(I) case CURLINFO_ ##I : { return pretty_key(http_curl_copystr(#I), sizeof(#I)-1, 0, 0); }
	switch (i)
	{
		/* CURLINFO_EFFECTIVE_URL			=	CURLINFO_STRING	+1, */
		CASE(EFFECTIVE_URL);
		/* CURLINFO_RESPONSE_CODE			=	CURLINFO_LONG	+2, */
#if LIBCURL_VERSION_NUM > 0x070a06
		CASE(RESPONSE_CODE);
#else
		CASE(HTTP_CODE);
#endif
		/* CURLINFO_TOTAL_TIME				=	CURLINFO_DOUBLE	+3, */
		CASE(TOTAL_TIME);
		/* CURLINFO_NAMELOOKUP_TIME			=	CURLINFO_DOUBLE	+4, */
		CASE(NAMELOOKUP_TIME);
		/* CURLINFO_CONNECT_TIME			=	CURLINFO_DOUBLE	+5, */
		CASE(CONNECT_TIME);
		/* CURLINFO_PRETRANSFER_TIME		=	CURLINFO_DOUBLE	+6, */
		CASE(PRETRANSFER_TIME);
		/* CURLINFO_SIZE_UPLOAD				=	CURLINFO_DOUBLE	+7, */
		CASE(SIZE_UPLOAD);
		/* CURLINFO_SIZE_DOWNLOAD			=	CURLINFO_DOUBLE	+8, */
		CASE(SIZE_DOWNLOAD);
		/* CURLINFO_SPEED_DOWNLOAD			=	CURLINFO_DOUBLE	+9, */
		CASE(SPEED_DOWNLOAD);
		/* CURLINFO_SPEED_UPLOAD			=	CURLINFO_DOUBLE	+10, */
		CASE(SPEED_UPLOAD);
		/* CURLINFO_HEADER_SIZE				=	CURLINFO_LONG	+11, */
		CASE(HEADER_SIZE);
		/* CURLINFO_REQUEST_SIZE			=	CURLINFO_LONG	+12, */
		CASE(REQUEST_SIZE);
		/* CURLINFO_SSL_VERIFYRESULT		=	CURLINFO_LONG	+13, */
		CASE(SSL_VERIFYRESULT);
		/* CURLINFO_FILETIME				=	CURLINFO_LONG	+14, */
		CASE(FILETIME);
		/* CURLINFO_CONTENT_LENGTH_DOWNLOAD	=	CURLINFO_DOUBLE	+15, */
		CASE(CONTENT_LENGTH_DOWNLOAD);
		/* CURLINFO_CONTENT_LENGTH_UPLOAD	=	CURLINFO_DOUBLE	+16, */
		CASE(CONTENT_LENGTH_UPLOAD);
		/* CURLINFO_STARTTRANSFER_TIME		=	CURLINFO_DOUBLE	+17, */
		CASE(STARTTRANSFER_TIME);
		/* CURLINFO_CONTENT_TYPE			=	CURLINFO_STRING	+18, */
		CASE(CONTENT_TYPE);
		/* CURLINFO_REDIRECT_TIME			=	CURLINFO_DOUBLE	+19, */
		CASE(REDIRECT_TIME);
		/* CURLINFO_REDIRECT_COUNT			=	CURLINFO_LONG	+20, */
		CASE(REDIRECT_COUNT);
		/* CURLINFO_PRIVATE					=	CURLINFO_STRING	+21, * (mike) /
		CASE(PRIVATE);
		/* CURLINFO_HTTP_CONNECTCODE		=	CURLINFO_LONG	+22, */
		CASE(HTTP_CONNECTCODE);
#if LIBCURL_VERSION_NUM > 0x070a07
		/* CURLINFO_HTTPAUTH_AVAIL			=	CURLINFO_LONG	+23, */
		CASE(HTTPAUTH_AVAIL);
		/* CURLINFO_PROXYAUTH_AVAIL			=	CURLINFO_LONG	+24, */
		CASE(PROXYAUTH_AVAIL);
#endif
	}
#undef CASE
	return NULL;
}
/* }}} */

/* {{{ static inline void http_curl_getinfo_ex(CURL, CURLINFO, zval *) */
static inline void _http_curl_getinfo_ex(CURL *ch, CURLINFO i, zval *array TSRMLS_DC)
{
	char *key;
	if (key = http_curl_getinfoname(i)) {
		switch (i & ~CURLINFO_MASK)
		{
			case CURLINFO_STRING:
			{
				char *c;
				if (CURLE_OK == curl_easy_getinfo(ch, i, &c)) {
					add_assoc_string(array, key, c ? c : "", 1);
				}
			}
			break;

			case CURLINFO_DOUBLE:
			{
				double d;
				if (CURLE_OK == curl_easy_getinfo(ch, i, &d)) {
					add_assoc_double(array, key, d);
				}
			}
			break;

			case CURLINFO_LONG:
			{
				long l;
				if (CURLE_OK == curl_easy_getinfo(ch, i, &l)) {
					add_assoc_long(array, key, l);
				}
			}
			break;
		}
	}
}
/* }}} */

/* {{{ static inline http_curl_getinfo(CURL, HashTable *) */
static inline void _http_curl_getinfo(CURL *ch, HashTable *info TSRMLS_DC)
{
	zval array;
	Z_ARRVAL(array) = info;

#define INFO(I) http_curl_getinfo_ex(ch, CURLINFO_ ##I , &array)
	/* CURLINFO_EFFECTIVE_URL			=	CURLINFO_STRING	+1, */
	INFO(EFFECTIVE_URL);
#if LIBCURL_VERSION_NUM > 0x070a06
	/* CURLINFO_RESPONSE_CODE			=	CURLINFO_LONG	+2, */
	INFO(RESPONSE_CODE);
#else
	INFO(HTTP_CODE);
#endif
	/* CURLINFO_TOTAL_TIME				=	CURLINFO_DOUBLE	+3, */
	INFO(TOTAL_TIME);
	/* CURLINFO_NAMELOOKUP_TIME			=	CURLINFO_DOUBLE	+4, */
	INFO(NAMELOOKUP_TIME);
	/* CURLINFO_CONNECT_TIME			=	CURLINFO_DOUBLE	+5, */
	INFO(CONNECT_TIME);
	/* CURLINFO_PRETRANSFER_TIME		=	CURLINFO_DOUBLE	+6, */
	INFO(PRETRANSFER_TIME);
	/* CURLINFO_SIZE_UPLOAD				=	CURLINFO_DOUBLE	+7, */
	INFO(SIZE_UPLOAD);
	/* CURLINFO_SIZE_DOWNLOAD			=	CURLINFO_DOUBLE	+8, */
	INFO(SIZE_DOWNLOAD);
	/* CURLINFO_SPEED_DOWNLOAD			=	CURLINFO_DOUBLE	+9, */
	INFO(SPEED_DOWNLOAD);
	/* CURLINFO_SPEED_UPLOAD			=	CURLINFO_DOUBLE	+10, */
	INFO(SPEED_UPLOAD);
	/* CURLINFO_HEADER_SIZE				=	CURLINFO_LONG	+11, */
	INFO(HEADER_SIZE);
	/* CURLINFO_REQUEST_SIZE			=	CURLINFO_LONG	+12, */
	INFO(REQUEST_SIZE);
	/* CURLINFO_SSL_VERIFYRESULT		=	CURLINFO_LONG	+13, */
	INFO(SSL_VERIFYRESULT);
	/* CURLINFO_FILETIME				=	CURLINFO_LONG	+14, */
	INFO(FILETIME);
	/* CURLINFO_CONTENT_LENGTH_DOWNLOAD	=	CURLINFO_DOUBLE	+15, */
	INFO(CONTENT_LENGTH_DOWNLOAD);
	/* CURLINFO_CONTENT_LENGTH_UPLOAD	=	CURLINFO_DOUBLE	+16, */
	INFO(CONTENT_LENGTH_UPLOAD);
	/* CURLINFO_STARTTRANSFER_TIME		=	CURLINFO_DOUBLE	+17, */
	INFO(STARTTRANSFER_TIME);
	/* CURLINFO_CONTENT_TYPE			=	CURLINFO_STRING	+18, */
	INFO(CONTENT_TYPE);
	/* CURLINFO_REDIRECT_TIME			=	CURLINFO_DOUBLE	+19, */
	INFO(REDIRECT_TIME);
	/* CURLINFO_REDIRECT_COUNT			=	CURLINFO_LONG	+20, */
	INFO(REDIRECT_COUNT);
	/* CURLINFO_PRIVATE					=	CURLINFO_STRING	+21, */
	INFO(PRIVATE);
	/* CURLINFO_HTTP_CONNECTCODE		=	CURLINFO_LONG	+22, */
	INFO(HTTP_CONNECTCODE);
#if LIBCURL_VERSION_NUM > 0x070a07
	/* CURLINFO_HTTPAUTH_AVAIL			=	CURLINFO_LONG	+23, */
	INFO(HTTPAUTH_AVAIL);
	/* CURLINFO_PROXYAUTH_AVAIL			=	CURLINFO_LONG	+24, */
	INFO(PROXYAUTH_AVAIL);
#endif
#undef INFO
}
/* }}} */

/* {{{ STATUS http_get_ex(CURL *, char *, HashTable *, HashTable *, char **, size_t *) */
PHP_HTTP_API STATUS _http_get_ex(CURL *ch, const char *URL, HashTable *options,
	HashTable *info, char **data, size_t *data_len TSRMLS_DC)
{
	zend_bool clean_curl = 0;

	http_curl_startup(ch, clean_curl, URL, options);
	curl_easy_setopt(ch, CURLOPT_NOBODY, 0);
	curl_easy_setopt(ch, CURLOPT_POST, 0);

	if (CURLE_OK != curl_easy_perform(ch)) {
		http_curl_cleanup(ch, clean_curl);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not perform request");
		return FAILURE;
	}

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
	curl_easy_setopt(ch, CURLOPT_POST, 0);

	if (CURLE_OK != curl_easy_perform(ch)) {
		http_curl_cleanup(ch, clean_curl);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not perform request");
		return FAILURE;
	}

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

	if (CURLE_OK != curl_easy_perform(ch)) {
		http_curl_cleanup(ch, clean_curl);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not perform request");
		return FAILURE;
	}

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
	smart_str qstr = {0};
	STATUS status;

	HTTP_URL_ARGSEP_OVERRIDE;
	if (php_url_encode_hash_ex(postarray, &qstr, NULL,0,NULL,0,NULL,0,NULL TSRMLS_CC) != SUCCESS) {
		if (qstr.c) {
			efree(qstr.c);
		}
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not encode post data");
		HTTP_URL_ARGSEP_RESTORE;
		return FAILURE;
	}
	smart_str_0(&qstr);
	HTTP_URL_ARGSEP_RESTORE;

	status = http_post_data_ex(ch, URL, qstr.c, qstr.len, options, info, data, data_len);

	if (qstr.c) {
		efree(qstr.c);
	}
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

	if (CURLE_OK != curl_easy_perform(ch)) {
		http_curl_cleanup(ch, clean_curl);
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Could not perform request");
		return FAILURE;
	}

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