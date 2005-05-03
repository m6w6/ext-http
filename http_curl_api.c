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

#include "phpstr/phpstr.h"

#include "php.h"
#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_curl_api.h"
#include "php_http_url_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http)

#if LIBCURL_VERSION_NUM >= 0x070c01
#	define http_curl_reset(ch) curl_easy_reset(ch)
#else
#	define http_curl_reset(ch)
#endif

#if LIBCURL_VERSION_NUM < 0x070c00
#	define http_curl_error(dummy) HTTP_G(curlerr)
#	define curl_easy_strerror(code) "unkown error"
#else
#	define http_curl_error(code) curl_easy_strerror(code)
#endif

#define http_curl_startup(ch, clean_curl, URL, options, response) \
	if (!ch) { \
		if (!(ch = curl_easy_init())) { \
			http_error(E_WARNING, HTTP_E_CURL, "Could not initialize curl"); \
			return FAILURE; \
		} \
		clean_curl = 1; \
	} else { \
		http_curl_reset(ch); \
	} \
	http_curl_setopts(ch, URL, options, response);

#define http_curl_perform(ch, clean_curl, response) \
	{ \
		CURLcode result; \
		if (CURLE_OK != (result = curl_easy_perform(ch))) { \
			http_error_ex(E_WARNING, HTTP_E_CURL, "Could not perform request: %s", curl_easy_strerror(result)); \
			http_curl_cleanup(ch, clean_curl, response); \
			return FAILURE; \
		} \
	}

#define http_curl_cleanup(ch, clean_curl, response) \
	zend_llist_clean(&HTTP_G(to_free)); \
	if (clean_curl) { \
		curl_easy_cleanup(ch); \
		ch = NULL; \
	} \
	phpstr_fix(PHPSTR(response))

#define http_curl_copystr(s) _http_curl_copystr((s) TSRMLS_CC)
static inline char *_http_curl_copystr(const char *str TSRMLS_DC);

#define http_curl_setopts(c, u, o, r) _http_curl_setopts((c), (u), (o), (r) TSRMLS_CC)
static void _http_curl_setopts(CURL *ch, const char *url, HashTable *options, phpstr *response TSRMLS_DC);

#define http_curl_getopt(o, k, t) _http_curl_getopt_ex((o), (k), sizeof(k), (t) TSRMLS_CC)
#define http_curl_getopt_ex(o, k, l, t) _http_curl_getopt_ex((o), (k), (l), (t) TSRMLS_CC)
static inline zval *_http_curl_getopt_ex(HashTable *options, char *key, size_t keylen, int type TSRMLS_DC);

static size_t http_curl_write_callback(char *, size_t, size_t, void *);
static int http_curl_progress_callback(void *, double, double, double, double);
static int http_curl_debug_callback(CURL *, curl_infotype, char *, size_t, void *);

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

/* {{{ static size_t http_curl_write_callback(char *, size_t, size_t, void *) */
static size_t http_curl_write_callback(char *buf, size_t len, size_t n, void *s)
{
	return s ? phpstr_append(PHPSTR(s), buf, len * n) : len * n;
}
/* }}} */

/* {{{ static int http_curl_progress_callback(void *, double, double, double, double) */
static int http_curl_progress_callback(void *data, double dltotal, double dlnow, double ultotal, double ulnow)
{
	zval *params_pass[4], params_local[4], retval, *func = (zval *) data;
	TSRMLS_FETCH();

	params_pass[0] = &params_local[0];
	params_pass[1] = &params_local[1];
	params_pass[2] = &params_local[2];
	params_pass[3] = &params_local[3];

	ZVAL_DOUBLE(params_pass[0], dltotal);
	ZVAL_DOUBLE(params_pass[1], dlnow);
	ZVAL_DOUBLE(params_pass[2], ultotal);
	ZVAL_DOUBLE(params_pass[3], ulnow);

	return call_user_function(EG(function_table), NULL, func, &retval, 4, params_pass TSRMLS_CC);
}
/* }}} */

static int http_curl_debug_callback(CURL *ch, curl_infotype type, char *string, size_t length, void *data)
{
	zval *params_pass[2], params_local[2], retval, *func = (zval *) data;
	TSRMLS_FETCH();

	params_pass[0] = &params_local[0];
	params_pass[1] = &params_local[1];

	ZVAL_LONG(params_pass[0], type);
	ZVAL_STRINGL(params_pass[1], string, length, 1);

	call_user_function(EG(function_table), NULL, func, &retval, 2, params_pass TSRMLS_CC);

	return 0;
}
/* {{{ static inline zval *http_curl_getopt(HashTable *, char *, size_t, int) */
static inline zval *_http_curl_getopt_ex(HashTable *options, char *key, size_t keylen, int type TSRMLS_DC)
{
	zval **zoption;

	if (!options || (SUCCESS != zend_hash_find(options, key, keylen, (void **) &zoption))) {
		return NULL;
	}

	if (Z_TYPE_PP(zoption) != type) {
		switch (type)
		{
			case IS_BOOL:	convert_to_boolean_ex(zoption);	break;
			case IS_LONG:	convert_to_long_ex(zoption);	break;
			case IS_DOUBLE:	convert_to_double_ex(zoption);	break;
			case IS_STRING:	convert_to_string_ex(zoption);	break;
			case IS_ARRAY:	convert_to_array_ex(zoption);	break;
			case IS_OBJECT: convert_to_object_ex(zoption);	break;
			default:
			break;
		}
	}

	return *zoption;
}
/* }}} */

/* {{{ static void http_curl_setopts(CURL *, char *, HashTable *, phpstr *) */
static void _http_curl_setopts(CURL *ch, const char *url, HashTable *options, phpstr *response TSRMLS_DC)
{
	zval *zoption;
	zend_bool range_req = 0;

#define HTTP_CURL_OPT(OPTION, p) curl_easy_setopt(ch, CURLOPT_##OPTION, (p))

	/* standard options */
	if (url) {
		HTTP_CURL_OPT(URL, url);
	}

	HTTP_CURL_OPT(HEADER, 0);
	HTTP_CURL_OPT(FILETIME, 1);
	HTTP_CURL_OPT(AUTOREFERER, 1);
	HTTP_CURL_OPT(WRITEFUNCTION, http_curl_write_callback);
	HTTP_CURL_OPT(HEADERFUNCTION, http_curl_write_callback);

	if (response) {
		HTTP_CURL_OPT(WRITEDATA, response);
		HTTP_CURL_OPT(WRITEHEADER, response);
	}

#if defined(ZTS) && (LIBCURL_VERSION_NUM >= 0x070a00)
	HTTP_CURL_OPT(NOSIGNAL, 1);
#endif
#if LIBCURL_VERSION_NUM < 0x070c00
	HTTP_CURL_OPT(ERRORBUFFER, HTTP_G(curlerr));
#endif

	/* progress callback */
	if (zoption = http_curl_getopt(options, "onprogress", 0)) {
		HTTP_CURL_OPT(NOPROGRESS, 0);
		HTTP_CURL_OPT(PROGRESSFUNCTION, http_curl_progress_callback);
		HTTP_CURL_OPT(PROGRESSDATA, zoption);
	} else {
		HTTP_CURL_OPT(NOPROGRESS, 1);
	}

	/* debug callback */
	if (zoption = http_curl_getopt(options, "ondebug", 0)) {
		HTTP_CURL_OPT(VERBOSE, 1);
		HTTP_CURL_OPT(DEBUGFUNCTION, http_curl_debug_callback);
		HTTP_CURL_OPT(DEBUGDATA, zoption);
	} else {
		HTTP_CURL_OPT(VERBOSE, 0);
	}

	/* proxy */
	if (zoption = http_curl_getopt(options, "proxyhost", IS_STRING)) {
		HTTP_CURL_OPT(PROXY, http_curl_copystr(Z_STRVAL_P(zoption)));
		/* port */
		if (zoption = http_curl_getopt(options, "proxyport", IS_LONG)) {
			HTTP_CURL_OPT(PROXYPORT, Z_LVAL_P(zoption));
		}
		/* user:pass */
		if (zoption = http_curl_getopt(options, "proxyauth", IS_STRING)) {
			HTTP_CURL_OPT(PROXYUSERPWD, http_curl_copystr(Z_STRVAL_P(zoption)));
		}
#if LIBCURL_VERSION_NUM >= 0x070a07
		/* auth method */
		if (zoption = http_curl_getopt(options, "proxyauthtype", IS_LONG)) {
			HTTP_CURL_OPT(PROXYAUTH, Z_LVAL_P(zoption));
		}
#endif
	}

	/* outgoing interface */
	if (zoption = http_curl_getopt(options, "interface", IS_STRING)) {
		HTTP_CURL_OPT(INTERFACE, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* another port */
	if (zoption = http_curl_getopt(options, "port", IS_LONG)) {
		HTTP_CURL_OPT(PORT, Z_LVAL_P(zoption));
	}

	/* auth */
	if (zoption = http_curl_getopt(options, "httpauth", IS_STRING)) {
		HTTP_CURL_OPT(USERPWD, http_curl_copystr(Z_STRVAL_P(zoption)));
	}
#if LIBCURL_VERSION_NUM >= 0x070a06
	if (zoption = http_curl_getopt(options, "httpauthtype", IS_LONG)) {
		HTTP_CURL_OPT(HTTPAUTH, Z_LVAL_P(zoption));
	}
#endif

	/* compress, empty string enables deflate and gzip */
	if (zoption = http_curl_getopt(options, "compress", IS_BOOL)) {
		if (Z_LVAL_P(zoption)) {
			HTTP_CURL_OPT(ENCODING, http_curl_copystr(""));
		}
	}

	/* redirects, defaults to 0 */
	if (zoption = http_curl_getopt(options, "redirect", IS_LONG)) {
		HTTP_CURL_OPT(FOLLOWLOCATION, Z_LVAL_P(zoption) ? 1 : 0);
		HTTP_CURL_OPT(MAXREDIRS, Z_LVAL_P(zoption));
		if (zoption = http_curl_getopt(options, "unrestrictedauth", IS_BOOL)) {
			HTTP_CURL_OPT(UNRESTRICTED_AUTH, Z_LVAL_P(zoption));
		}
	} else {
		HTTP_CURL_OPT(FOLLOWLOCATION, 0);
	}

	/* referer */
	if (zoption = http_curl_getopt(options, "referer", IS_STRING)) {
		HTTP_CURL_OPT(REFERER, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if (zoption = http_curl_getopt(options, "useragent", IS_STRING)) {
		HTTP_CURL_OPT(USERAGENT, http_curl_copystr(Z_STRVAL_P(zoption)));
	} else {
		HTTP_CURL_OPT(USERAGENT,
			"PECL::HTTP/" HTTP_PEXT_VERSION " (PHP/" PHP_VERSION ")");
	}

	/* additional headers, array('name' => 'value') */
	if (zoption = http_curl_getopt(options, "headers", IS_ARRAY)) {
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
			HTTP_CURL_OPT(HTTPHEADER, headers);
		}
	}

	/* cookies, array('name' => 'value') */
	if (zoption = http_curl_getopt(options, "cookies", IS_ARRAY)) {
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
			HTTP_CURL_OPT(COOKIE, http_curl_copystr(qstr->data));
		}
		phpstr_free(qstr);
	}

	/* cookiestore */
	if (zoption = http_curl_getopt(options, "cookiestore", IS_STRING)) {
		HTTP_CURL_OPT(COOKIEFILE, http_curl_copystr(Z_STRVAL_P(zoption)));
		HTTP_CURL_OPT(COOKIEJAR, http_curl_copystr(Z_STRVAL_P(zoption)));
	}

	/* resume */
	if (zoption = http_curl_getopt(options, "resume", IS_LONG)) {
		range_req = 1;
		HTTP_CURL_OPT(RESUME_FROM, Z_LVAL_P(zoption));
	}

	/* maxfilesize */
	if (zoption = http_curl_getopt(options, "maxfilesize", IS_LONG)) {
		HTTP_CURL_OPT(MAXFILESIZE, Z_LVAL_P(zoption));
	}

	/* lastmodified */
	if (zoption = http_curl_getopt(options, "lastmodified", IS_LONG)) {
		HTTP_CURL_OPT(TIMECONDITION, range_req ? CURL_TIMECOND_IFUNMODSINCE : CURL_TIMECOND_IFMODSINCE);
		HTTP_CURL_OPT(TIMEVALUE, Z_LVAL_P(zoption));
	}

	/* timeout */
	if (zoption = http_curl_getopt(options, "timeout", IS_LONG)) {
		HTTP_CURL_OPT(TIMEOUT, Z_LVAL_P(zoption));
	}

	/* connecttimeout, defaults to 1 */
	if (zoption = http_curl_getopt(options, "connecttimeout", IS_LONG)) {
		HTTP_CURL_OPT(CONNECTTIMEOUT, Z_LVAL_P(zoption));
	} else {
		HTTP_CURL_OPT(CONNECTTIMEOUT, 1);
	}

	/* ssl */
	if (zoption = http_curl_getopt(options, "ssl", IS_ARRAY)) {
#define HTTP_CURL_OPT_STRING(keyname) HTTP_CURL_OPT_STRING_EX(keyname, keyname)
#define HTTP_CURL_OPT_SSL_STRING(keyname) HTTP_CURL_OPT_STRING_EX(keyname, SSL##keyname)
#define HTTP_CURL_OPT_SSL_STRING_(keyname) HTTP_CURL_OPT_STRING_EX(keyname, SSL_##keyname)
#define HTTP_CURL_OPT_STRING_EX(keyname, optname) \
	if (!strcasecmp(key, #keyname)) { \
		convert_to_string_ex(param); \
		HTTP_CURL_OPT(optname, http_curl_copystr(Z_STRVAL_PP(param))); \
		key = NULL; \
		continue; \
	}
#define HTTP_CURL_OPT_LONG(keyname) HTTP_OPT_SSL_LONG_EX(keyname, keyname)
#define HTTP_CURL_OPT_SSL_LONG(keyname) HTTP_CURL_OPT_LONG_EX(keyname, SSL##keyname)
#define HTTP_CURL_OPT_SSL_LONG_(keyname) HTTP_CURL_OPT_LONG_EX(keyname, SSL_##keyname)
#define HTTP_CURL_OPT_LONG_EX(keyname, optname) \
	if (!strcasecmp(key, #keyname)) { \
		convert_to_long_ex(param); \
		HTTP_CURL_OPT(optname, Z_LVAL_PP(param)); \
		key = NULL; \
		continue; \
	}

		long idx;
		char *key = NULL;
		zval **param;

		FOREACH_KEYVAL(zoption, key, idx, param) {
			if (key) {fprintf(stderr, "%s\n", key);
				HTTP_CURL_OPT_SSL_STRING(CERT);
#if LIBCURL_VERSION_NUM >= 0x070903
				HTTP_CURL_OPT_SSL_STRING(CERTTYPE);
#endif
				HTTP_CURL_OPT_SSL_STRING(CERTPASSWD);

				HTTP_CURL_OPT_SSL_STRING(KEY);
				HTTP_CURL_OPT_SSL_STRING(KEYTYPE);
				HTTP_CURL_OPT_SSL_STRING(KEYPASSWD);

				HTTP_CURL_OPT_SSL_STRING(ENGINE);
				HTTP_CURL_OPT_SSL_LONG(VERSION);

				HTTP_CURL_OPT_SSL_LONG_(VERIFYPEER);
				HTTP_CURL_OPT_SSL_LONG_(VERIFYHOST);
				HTTP_CURL_OPT_SSL_STRING_(CIPHER_LIST);


				HTTP_CURL_OPT_STRING(CAINFO);
#if LIBCURL_VERSION_NUM >= 0x070908
				HTTP_CURL_OPT_STRING(CAPATH);
#endif
				HTTP_CURL_OPT_STRING(RANDOM_FILE);
				HTTP_CURL_OPT_STRING(EGDSOCKET);

				/* reset key */
				key = NULL;
			}
		}
	} else {
		/* disable SSL verification by default */
		HTTP_CURL_OPT(SSL_VERIFYPEER, 0);
		HTTP_CURL_OPT(SSL_VERIFYHOST, 0);
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

/* {{{ STATUS http_get_ex(CURL *, char *, HashTable *, HashTable *, phpstr *) */
PHP_HTTP_API STATUS _http_get_ex(CURL *ch, const char *URL, HashTable *options, HashTable *info, phpstr *response TSRMLS_DC)
{
	zend_bool clean_curl = 0;

	http_curl_startup(ch, clean_curl, URL, options, response);
	curl_easy_setopt(ch, CURLOPT_HTTPGET, 1);
	http_curl_perform(ch, clean_curl, response);

	if (info) {
		http_curl_getinfo(ch, info);
	}

	http_curl_cleanup(ch, clean_curl, response);

	return SUCCESS;
}

/* {{{ STATUS http_head_ex(CURL *, char *, HashTable *, HashTable *, phpstr *) */
PHP_HTTP_API STATUS _http_head_ex(CURL *ch, const char *URL, HashTable *options,HashTable *info, phpstr *response TSRMLS_DC)
{
	zend_bool clean_curl = 0;

	http_curl_startup(ch, clean_curl, URL, options, response);
	curl_easy_setopt(ch, CURLOPT_NOBODY, 1);
	http_curl_perform(ch, clean_curl, response);

	if (info) {
		http_curl_getinfo(ch, info);
	}

	http_curl_cleanup(ch, clean_curl, response);

	return SUCCESS;
}

/* {{{ STATUS http_post_data_ex(CURL *, char *, char *, size_t, HashTable *, HashTable *, phpstr *) */
PHP_HTTP_API STATUS _http_post_data_ex(CURL *ch, const char *URL, char *postdata,
	size_t postdata_len, HashTable *options, HashTable *info, phpstr *response TSRMLS_DC)
{
	zend_bool clean_curl = 0;

	http_curl_startup(ch, clean_curl, URL, options, response);
	curl_easy_setopt(ch, CURLOPT_POST, 1);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, postdata);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, postdata_len);
	http_curl_perform(ch, clean_curl, response);

	if (info) {
		http_curl_getinfo(ch, info);
	}

	http_curl_cleanup(ch, clean_curl, response);

	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_post_array_ex(CURL *, char *, HashTable *, HashTable *, HashTable *, phpstr *) */
PHP_HTTP_API STATUS _http_post_array_ex(CURL *ch, const char *URL, HashTable *postarray,
	HashTable *options, HashTable *info, phpstr *response TSRMLS_DC)
{
	STATUS status;
	char *encoded;
	size_t encoded_len;

	if (SUCCESS != http_urlencode_hash_ex(postarray, 1, NULL, 0, &encoded, &encoded_len)) {
		http_error(E_WARNING, HTTP_E_ENCODE, "Could not encode post data");
		return FAILURE;
	}

	status = http_post_data_ex(ch, URL, encoded, encoded_len, options, info, response);
	efree(encoded);

	return status;
}
/* }}} */

/* {{{ STATUS http_post_curldata_ex(CURL *, char *, curl_httppost *, HashTable *, HashTable *, phpstr *) */
PHP_HTTP_API STATUS _http_post_curldata_ex(CURL *ch, const char *URL, struct curl_httppost *curldata,
	HashTable *options, HashTable *info, phpstr *response TSRMLS_DC)
{
	zend_bool clean_curl = 0;

	http_curl_startup(ch, clean_curl, URL, options, response);
	curl_easy_setopt(ch, CURLOPT_POST, 1);
	curl_easy_setopt(ch, CURLOPT_HTTPPOST, curldata);
	http_curl_perform(ch, clean_curl, response);

	if (info) {
		http_curl_getinfo(ch, info);
	}

	http_curl_cleanup(ch, clean_curl, response);

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
