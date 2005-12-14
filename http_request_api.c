/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2005, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#define HTTP_WANT_CURL
#include "php_http.h"

#ifdef HTTP_HAVE_CURL

#include "php_http_api.h"
#include "php_http_request_api.h"
#include "php_http_url_api.h"

#ifdef ZEND_ENGINE_2
#	include "php_http_request_object.h"
#endif

ZEND_EXTERN_MODULE_GLOBALS(http);

/* {{{ cruft for thread safe SSL crypto locks */
#if defined(ZTS) && defined(HTTP_HAVE_SSL)
#	ifdef PHP_WIN32
#		define HTTP_NEED_SSL_TSL
#		define HTTP_NEED_OPENSSL_TSL
#		include <openssl/crypto.h>
#	else /* !PHP_WIN32 */
#		if defined(HTTP_HAVE_OPENSSL)
#			if defined(HAVE_OPENSSL_CRYPTO_H)
#				define HTTP_NEED_SSL_TSL
#				define HTTP_NEED_OPENSSL_TSL
#				include <openssl/crypto.h>
#			else
#				warning \
					"libcurl was compiled with OpenSSL support, but configure could not find " \
					"openssl/crypto.h; thus no SSL crypto locking callbacks will be set, which may " \
					"cause random crashes on SSL requests"
#			endif
#		elif defined(HTTP_HAVE_GNUTLS)
#			if defined(HAVE_GCRYPT_H)
#				define HTTP_NEED_SSL_TSL
#				define HTTP_NEED_GNUTLS_TSL
#				include <gcrypt.h>
#			else
#				warning \
					"libcurl was compiled with GnuTLS support, but configure could not find " \
					"gcrypt.h; thus no SSL crypto locking callbacks will be set, which may " \
					"cause random crashes on SSL requests"
#			endif
#		else
#			warning \
				"libcurl was compiled with SSL support, but configure could not determine which" \
				"library was used; thus no SSL crypto locking callbacks will be set, which may " \
				"cause random crashes on SSL requests"
#		endif /* HTTP_HAVE_OPENSSL || HTTP_HAVE_GNUTLS */
#	endif /* PHP_WIN32 */
#endif /* ZTS && HTTP_HAVE_SSL */

#ifdef HTTP_NEED_SSL_TSL
static inline void http_ssl_init(void);
static inline void http_ssl_cleanup(void);
#endif
/* }}} */

/* {{{ MINIT */
PHP_MINIT_FUNCTION(http_request)
{
#ifdef HTTP_NEED_SSL_TSL
	http_ssl_init();
#endif

	if (CURLE_OK != curl_global_init(CURL_GLOBAL_ALL)) {
		return FAILURE;
	}
	
	HTTP_LONG_CONSTANT("HTTP_AUTH_BASIC", CURLAUTH_BASIC);
	HTTP_LONG_CONSTANT("HTTP_AUTH_DIGEST", CURLAUTH_DIGEST);
	HTTP_LONG_CONSTANT("HTTP_AUTH_NTLM", CURLAUTH_NTLM);
	HTTP_LONG_CONSTANT("HTTP_AUTH_ANY", CURLAUTH_ANY);

	return SUCCESS;
}
/* }}} */

/* {{{ MSHUTDOWN */
PHP_MSHUTDOWN_FUNCTION(http_request)
{
	curl_global_cleanup();
#ifdef HTTP_NEED_SSL_TSL
	http_ssl_cleanup();
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ MACROS */
#ifndef HAVE_CURL_EASY_STRERROR
#	define curl_easy_strerror(dummy) "unkown error"
#endif

#define HTTP_CURL_INFO(I) HTTP_CURL_INFO_EX(I, I)
#define HTTP_CURL_INFO_EX(I, X) \
	switch (CURLINFO_ ##I & ~CURLINFO_MASK) \
	{ \
		case CURLINFO_STRING: \
		{ \
			char *c; \
			if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_ ##I, &c)) { \
				char *key = estrndup(#X, lenof(#X)); \
				add_assoc_string(&array, pretty_key(key, lenof(#X), 0, 0), c ? c : "", 1); \
				efree(key); \
			} \
		} \
		break; \
\
		case CURLINFO_DOUBLE: \
		{ \
			double d; \
			if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_ ##I, &d)) { \
				char *key = estrndup(#X, lenof(#X)); \
				add_assoc_double(&array, pretty_key(key, lenof(#X), 0, 0), d); \
				efree(key); \
			} \
		} \
		break; \
\
		case CURLINFO_LONG: \
		{ \
			long l; \
			if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_ ##I, &l)) { \
				char *key = estrndup(#X, lenof(#X)); \
				add_assoc_long(&array, pretty_key(key, lenof(#X), 0, 0), l); \
				efree(key); \
			} \
		} \
		break; \
\
		case CURLINFO_SLIST: \
		{ \
			struct curl_slist *l, *p; \
			if (CURLE_OK == curl_easy_getinfo(request->ch, CURLINFO_ ##I, &l)) { \
				zval *subarray; \
				char *key = estrndup(#X, lenof(#X)); \
				MAKE_STD_ZVAL(subarray); \
				array_init(subarray); \
				for (p = l; p; p = p->next) { \
					add_next_index_string(subarray, p->data, 1); \
				} \
				add_assoc_zval(&array, pretty_key(key, lenof(#X), 0, 0), subarray); \
				curl_slist_free_all(l); \
				efree(key); \
			} \
		} \
	}

#define HTTP_CURL_OPT(OPTION, p) curl_easy_setopt(request->ch, CURLOPT_##OPTION, (p))
#define HTTP_CURL_OPT_STRING(keyname) HTTP_CURL_OPT_STRING_EX(keyname, keyname)
#define HTTP_CURL_OPT_SSL_STRING(keyname) HTTP_CURL_OPT_STRING_EX(keyname, SSL##keyname)
#define HTTP_CURL_OPT_SSL_STRING_(keyname) HTTP_CURL_OPT_STRING_EX(keyname, SSL_##keyname)
#define HTTP_CURL_OPT_STRING_EX(keyname, optname) \
	if (!strcasecmp(key, #keyname)) { \
		convert_to_string(*param); \
		HTTP_CURL_OPT(optname, Z_STRVAL_PP(param)); \
		key = NULL; \
		continue; \
	}
#define HTTP_CURL_OPT_LONG(keyname) HTTP_OPT_SSL_LONG_EX(keyname, keyname)
#define HTTP_CURL_OPT_SSL_LONG(keyname) HTTP_CURL_OPT_LONG_EX(keyname, SSL##keyname)
#define HTTP_CURL_OPT_SSL_LONG_(keyname) HTTP_CURL_OPT_LONG_EX(keyname, SSL_##keyname)
#define HTTP_CURL_OPT_LONG_EX(keyname, optname) \
	if (!strcasecmp(key, #keyname)) { \
		convert_to_long(*param); \
		HTTP_CURL_OPT(optname, Z_LVAL_PP(param)); \
		key = NULL; \
		continue; \
	}
/* }}} */

/* {{{ forward declarations */
#define http_request_option(r, o, k, t) _http_request_option_ex((r), (o), (k), sizeof(k), (t) TSRMLS_CC)
#define http_request_option_ex(r, o, k, l, t) _http_request_option_ex((r), (o), (k), (l), (t) TSRMLS_CC)
static inline zval *_http_request_option_ex(http_request *request, HashTable *options, char *key, size_t keylen, int type TSRMLS_DC);

static size_t http_curl_read_callback(void *, size_t, size_t, void *);
static int http_curl_progress_callback(void *, double, double, double, double);
static int http_curl_raw_callback(CURL *, curl_infotype, char *, size_t, void *);
static int http_curl_dummy_callback(char *data, size_t n, size_t l, void *s) { return n*l; }
static curlioerr http_curl_ioctl_callback(CURL *, curliocmd, void *);
/* }}} */

/* {{{ http_request *http_request_init(http_request *) */
PHP_HTTP_API http_request *_http_request_init_ex(http_request *request, CURL *ch, http_request_method meth, const char *url ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	http_request *r;
	
	if (request) {
		r = request;
	} else {
		r = emalloc_rel(sizeof(http_request));
	}
	memset(r, 0, sizeof(http_request));
	
	r->ch = ch;
	r->url = (url && *url) ? estrdup(url) : NULL;
	r->meth = (meth > 0) ? meth : HTTP_GET;
	
	phpstr_init(&r->conv.request);
	phpstr_init_ex(&r->conv.response, HTTP_CURLBUF_SIZE, 0);
	phpstr_init(&r->_cache.cookies);
	zend_hash_init(&r->_cache.options, 0, NULL, ZVAL_PTR_DTOR, 0);
	
	TSRMLS_SET_CTX(r->tsrm_ls);
	
	return r;
}
/* }}} */

/* {{{ void http_request_dtor(http_request *) */
PHP_HTTP_API void _http_request_dtor(http_request *request)
{
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	if (request->ch) {
		/* avoid nasty segfaults with already cleaned up callbacks */
		curl_easy_setopt(request->ch, CURLOPT_NOPROGRESS, 1);
		curl_easy_setopt(request->ch, CURLOPT_PROGRESSFUNCTION, NULL);
		curl_easy_setopt(request->ch, CURLOPT_VERBOSE, 0);
		curl_easy_setopt(request->ch, CURLOPT_DEBUGFUNCTION, NULL);
		curl_easy_cleanup(request->ch);
		request->ch = NULL;
	}
	
	http_request_reset(request);
	
	phpstr_dtor(&request->_cache.cookies);
	zend_hash_destroy(&request->_cache.options);
	if (request->_cache.headers) {
		curl_slist_free_all(request->_cache.headers);
		request->_cache.headers = NULL;
	}
	if (request->_progress_callback) {
		zval_ptr_dtor(&request->_progress_callback);
		request->_progress_callback = NULL;
	}
}
/* }}} */

/* {{{ void http_request_free(http_request **) */
PHP_HTTP_API void _http_request_free(http_request **request)
{
	if (*request) {
		TSRMLS_FETCH_FROM_CTX((*request)->tsrm_ls);
		http_request_body_free(&(*request)->body);
		http_request_dtor(*request);
		efree(*request);
		*request = NULL;
	}
}
/* }}} */

/* {{{ void http_request_reset(http_request *) */
PHP_HTTP_API void _http_request_reset(http_request *request)
{
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	STR_SET(request->url, NULL);
	request->conv.last_type = 0;
	phpstr_dtor(&request->conv.request);
	phpstr_dtor(&request->conv.response);
	http_request_body_dtor(request->body);
}
/* }}} */

/* {{{ void http_request_defaults(http_request *) */
PHP_HTTP_API void _http_request_defaults(http_request *request)
{
	if (request->ch) {
#ifdef HAVE_CURL_EASY_RESET
		curl_easy_reset(request->ch);
#endif
#if defined(ZTS)
		HTTP_CURL_OPT(NOSIGNAL, 1);
#endif
		HTTP_CURL_OPT(PRIVATE, request);
		HTTP_CURL_OPT(ERRORBUFFER, request->_error);
		HTTP_CURL_OPT(HEADER, 0);
		HTTP_CURL_OPT(FILETIME, 1);
		HTTP_CURL_OPT(AUTOREFERER, 1);
		HTTP_CURL_OPT(VERBOSE, 1);
		HTTP_CURL_OPT(HEADERFUNCTION, NULL);
		HTTP_CURL_OPT(DEBUGFUNCTION, http_curl_raw_callback);
		HTTP_CURL_OPT(READFUNCTION, http_curl_read_callback);
		HTTP_CURL_OPT(IOCTLFUNCTION, http_curl_ioctl_callback);
		HTTP_CURL_OPT(WRITEFUNCTION, http_curl_dummy_callback);
		HTTP_CURL_OPT(PROGRESSFUNCTION, http_curl_progress_callback);
		HTTP_CURL_OPT(URL, NULL);
		HTTP_CURL_OPT(NOPROGRESS, 1);
		HTTP_CURL_OPT(PROXY, NULL);
		HTTP_CURL_OPT(PROXYPORT, 0);
		HTTP_CURL_OPT(PROXYUSERPWD, NULL);
		HTTP_CURL_OPT(PROXYAUTH, 0);
		HTTP_CURL_OPT(INTERFACE, NULL);
		HTTP_CURL_OPT(PORT, 0);
		HTTP_CURL_OPT(USERPWD, NULL);
		HTTP_CURL_OPT(HTTPAUTH, 0);
		HTTP_CURL_OPT(ENCODING, 0);
		HTTP_CURL_OPT(FOLLOWLOCATION, 0);
		HTTP_CURL_OPT(UNRESTRICTED_AUTH, 0);
		HTTP_CURL_OPT(REFERER, NULL);
		HTTP_CURL_OPT(USERAGENT, "PECL::HTTP/" PHP_EXT_HTTP_VERSION " (PHP/" PHP_VERSION ")");
		HTTP_CURL_OPT(HTTPHEADER, NULL);
		HTTP_CURL_OPT(COOKIE, NULL);
		HTTP_CURL_OPT(COOKIEFILE, NULL);
		HTTP_CURL_OPT(COOKIEJAR, NULL);
		HTTP_CURL_OPT(RESUME_FROM, 0);
		HTTP_CURL_OPT(MAXFILESIZE, 0);
		HTTP_CURL_OPT(TIMECONDITION, 0);
		HTTP_CURL_OPT(TIMEVALUE, 0);
		HTTP_CURL_OPT(TIMEOUT, 0);
		HTTP_CURL_OPT(CONNECTTIMEOUT, 3);
		HTTP_CURL_OPT(SSLCERT, NULL);
		HTTP_CURL_OPT(SSLCERTTYPE, NULL);
		HTTP_CURL_OPT(SSLCERTPASSWD, NULL);
		HTTP_CURL_OPT(SSLKEY, NULL);
		HTTP_CURL_OPT(SSLKEYTYPE, NULL);
		HTTP_CURL_OPT(SSLKEYPASSWD, NULL);
		HTTP_CURL_OPT(SSLENGINE, NULL);
		HTTP_CURL_OPT(SSLVERSION, 0);
		HTTP_CURL_OPT(SSL_VERIFYPEER, 0);
		HTTP_CURL_OPT(SSL_VERIFYHOST, 0);
		HTTP_CURL_OPT(SSL_CIPHER_LIST, NULL);
		HTTP_CURL_OPT(CAINFO, NULL);
		HTTP_CURL_OPT(CAPATH, NULL);
		HTTP_CURL_OPT(RANDOM_FILE, NULL);
		HTTP_CURL_OPT(EGDSOCKET, NULL);
		HTTP_CURL_OPT(POSTFIELDS, NULL);
		HTTP_CURL_OPT(POSTFIELDSIZE, 0);
		HTTP_CURL_OPT(HTTPPOST, NULL);
		HTTP_CURL_OPT(IOCTLDATA, NULL);
		HTTP_CURL_OPT(READDATA, NULL);
		HTTP_CURL_OPT(INFILESIZE, 0);
	}
}
/* }}} */

PHP_HTTP_API void _http_request_set_progress_callback(http_request *request, zval *cb)
{
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	if (request->_progress_callback) {
		zval_ptr_dtor(&request->_progress_callback);
	}
	if (cb) {
		ZVAL_ADDREF(cb);
	}
	request->_progress_callback = cb;
}

/* {{{ STATUS http_request_prepare(http_request *, HashTable *) */
PHP_HTTP_API STATUS _http_request_prepare(http_request *request, HashTable *options)
{
	zval *zoption;
	zend_bool range_req = 0;

	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	HTTP_CHECK_CURL_INIT(request->ch, curl_easy_init(), return FAILURE);
	
	http_request_defaults(request);
	
	/* set options */
	HTTP_CURL_OPT(DEBUGDATA, request);
	HTTP_CURL_OPT(URL, request->url);

	/* progress callback */
	if ((zoption = http_request_option(request, options, "onprogress", 0))) {
		HTTP_CURL_OPT(NOPROGRESS, 0);
		HTTP_CURL_OPT(PROGRESSDATA, request);
		http_request_set_progress_callback(request, zoption);
	}

	/* proxy */
	if ((zoption = http_request_option(request, options, "proxyhost", IS_STRING))) {
		HTTP_CURL_OPT(PROXY, Z_STRVAL_P(zoption));
		/* port */
		if ((zoption = http_request_option(request, options, "proxyport", IS_LONG))) {
			HTTP_CURL_OPT(PROXYPORT, Z_LVAL_P(zoption));
		}
		/* user:pass */
		if ((zoption = http_request_option(request, options, "proxyauth", IS_STRING))) {
			HTTP_CURL_OPT(PROXYUSERPWD, Z_STRVAL_P(zoption));
		}
		/* auth method */
		if ((zoption = http_request_option(request, options, "proxyauthtype", IS_LONG))) {
			HTTP_CURL_OPT(PROXYAUTH, Z_LVAL_P(zoption));
		}
	}

	/* outgoing interface */
	if ((zoption = http_request_option(request, options, "interface", IS_STRING))) {
		HTTP_CURL_OPT(INTERFACE, Z_STRVAL_P(zoption));
	}

	/* another port */
	if ((zoption = http_request_option(request, options, "port", IS_LONG))) {
		HTTP_CURL_OPT(PORT, Z_LVAL_P(zoption));
	}

	/* auth */
	if ((zoption = http_request_option(request, options, "httpauth", IS_STRING))) {
		HTTP_CURL_OPT(USERPWD, Z_STRVAL_P(zoption));
	}
	if ((zoption = http_request_option(request, options, "httpauthtype", IS_LONG))) {
		HTTP_CURL_OPT(HTTPAUTH, Z_LVAL_P(zoption));
	}

	/* compress, empty string enables all supported if libcurl was build with zlib support */
	if ((zoption = http_request_option(request, options, "compress", IS_BOOL)) && Z_LVAL_P(zoption)) {
#ifdef HTTP_HAVE_CURL_ZLIB
		HTTP_CURL_OPT(ENCODING, "gzip, deflate");
#else
		HTTP_CURL_OPT(ENCODING, "gzip;q=1.0, deflate;q=0.5, *;q=0.1");
#endif
	}

	/* redirects, defaults to 0 */
	if ((zoption = http_request_option(request, options, "redirect", IS_LONG))) {
		HTTP_CURL_OPT(FOLLOWLOCATION, Z_LVAL_P(zoption) ? 1 : 0);
		HTTP_CURL_OPT(MAXREDIRS, Z_LVAL_P(zoption));
		if ((zoption = http_request_option(request, options, "unrestrictedauth", IS_BOOL))) {
			HTTP_CURL_OPT(UNRESTRICTED_AUTH, Z_LVAL_P(zoption));
		}
	}

	/* referer */
	if ((zoption = http_request_option(request, options, "referer", IS_STRING))) {
		HTTP_CURL_OPT(REFERER, Z_STRVAL_P(zoption));
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if ((zoption = http_request_option(request, options, "useragent", IS_STRING))) {
		HTTP_CURL_OPT(USERAGENT, Z_STRVAL_P(zoption));
	}

	/* additional headers, array('name' => 'value') */
	if ((zoption = http_request_option(request, options, "headers", IS_ARRAY))) {
		char *header_key;
		ulong header_idx;
		HashPosition pos;

		if (request->_cache.headers) {
			curl_slist_free_all(request->_cache.headers);
			request->_cache.headers = NULL;
		}
		
		FOREACH_KEY(pos, zoption, header_key, header_idx) {
			if (header_key) {
				zval **header_val;
				if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_P(zoption), (void **) &header_val, &pos)) {
					char header[1024] = {0};
					zval *cpy, *val = convert_to_type_ex(IS_STRING, *header_val, &cpy);
					
					snprintf(header, 1023, "%s: %s", header_key, Z_STRVAL_P(val));
					request->_cache.headers = curl_slist_append(request->_cache.headers, header);
					
					if (cpy) {
						zval_ptr_dtor(&cpy);
					}
				}

				/* reset */
				header_key = NULL;
			}
		}
	}

	/* cookies, array('name' => 'value') */
	if ((zoption = http_request_option(request, options, "cookies", IS_ARRAY))) {
		char *cookie_key = NULL;
		ulong cookie_idx = 0;
		HashPosition pos;
		
		phpstr_dtor(&request->_cache.cookies);

		FOREACH_KEY(pos, zoption, cookie_key, cookie_idx) {
			if (cookie_key) {
				zval **cookie_val;
				if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_P(zoption), (void **) &cookie_val, &pos)) {
					zval *cpy, *val = convert_to_type_ex(IS_STRING, *cookie_val, &cpy);
					
					phpstr_appendf(&request->_cache.cookies, "%s=%s; ", cookie_key, Z_STRVAL_P(val));
					
					if (cpy) {
						zval_ptr_dtor(&cpy);
					}
				}

				/* reset */
				cookie_key = NULL;
			}
		}

		if (request->_cache.cookies.used) {
			phpstr_fix(&request->_cache.cookies);
			HTTP_CURL_OPT(COOKIE, request->_cache.cookies.data);
		}
	}

	/* session cookies */
	if ((zoption = http_request_option(request, options, "cookiesession", IS_BOOL))) {
		if (Z_LVAL_P(zoption)) {
			/* accept cookies for this session */
			HTTP_CURL_OPT(COOKIEFILE, "");
		} else {
			/* reset session cookies */
			HTTP_CURL_OPT(COOKIESESSION, 1);
		}
	}

	/* cookiestore, read initial cookies from that file and store cookies back into that file */
	if ((zoption = http_request_option(request, options, "cookiestore", IS_STRING)) && Z_STRLEN_P(zoption)) {
		HTTP_CURL_OPT(COOKIEFILE, Z_STRVAL_P(zoption));
		HTTP_CURL_OPT(COOKIEJAR, Z_STRVAL_P(zoption));
	}

	/* resume */
	if ((zoption = http_request_option(request, options, "resume", IS_LONG)) && (Z_LVAL_P(zoption) != 0)) {
		range_req = 1;
		HTTP_CURL_OPT(RESUME_FROM, Z_LVAL_P(zoption));
	}

	/* maxfilesize */
	if ((zoption = http_request_option(request, options, "maxfilesize", IS_LONG))) {
		HTTP_CURL_OPT(MAXFILESIZE, Z_LVAL_P(zoption));
	}

	/* lastmodified */
	if ((zoption = http_request_option(request, options, "lastmodified", IS_LONG))) {
		if (Z_LVAL_P(zoption)) {
			if (Z_LVAL_P(zoption) > 0) {
				HTTP_CURL_OPT(TIMEVALUE, Z_LVAL_P(zoption));
			} else {
				HTTP_CURL_OPT(TIMEVALUE, time(NULL) + Z_LVAL_P(zoption));
			}
			HTTP_CURL_OPT(TIMECONDITION, range_req ? CURL_TIMECOND_IFUNMODSINCE : CURL_TIMECOND_IFMODSINCE);
		} else {
			HTTP_CURL_OPT(TIMECONDITION, CURL_TIMECOND_NONE);
		}
	}

	/* timeout, defaults to 0 */
	if ((zoption = http_request_option(request, options, "timeout", IS_LONG))) {
		HTTP_CURL_OPT(TIMEOUT, Z_LVAL_P(zoption));
	}

	/* connecttimeout, defaults to 3 */
	if ((zoption = http_request_option(request, options, "connecttimeout", IS_LONG))) {
		HTTP_CURL_OPT(CONNECTTIMEOUT, Z_LVAL_P(zoption));
	}

	/* ssl */
	if ((zoption = http_request_option(request, options, "ssl", IS_ARRAY))) {
		ulong idx;
		char *key = NULL;
		zval **param;
		HashPosition pos;

		FOREACH_KEYVAL(pos, zoption, key, idx, param) {
			if (key) {
				HTTP_CURL_OPT_SSL_STRING(CERT);
				HTTP_CURL_OPT_SSL_STRING(CERTTYPE);
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
				HTTP_CURL_OPT_STRING(CAPATH);
				HTTP_CURL_OPT_STRING(RANDOM_FILE);
				HTTP_CURL_OPT_STRING(EGDSOCKET);

				/* reset key */
				key = NULL;
			}
		}
	}

	/* request method */
	switch (request->meth)
	{
		case HTTP_GET:
			curl_easy_setopt(request->ch, CURLOPT_HTTPGET, 1);
		break;

		case HTTP_HEAD:
			curl_easy_setopt(request->ch, CURLOPT_NOBODY, 1);
		break;

		case HTTP_POST:
			curl_easy_setopt(request->ch, CURLOPT_POST, 1);
		break;

		case HTTP_PUT:
			curl_easy_setopt(request->ch, CURLOPT_UPLOAD, 1);
		break;

		default:
			if (http_request_method_exists(0, request->meth, NULL)) {
				curl_easy_setopt(request->ch, CURLOPT_CUSTOMREQUEST, http_request_method_name(request->meth));
			} else {
				http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Unsupported request method: %d (%s)", request->meth, request->url);
				return FAILURE;
			}
		break;
	}

	/* attach request body */
	if (request->body && (request->meth != HTTP_GET) && (request->meth != HTTP_HEAD)) {
		switch (request->body->type)
		{
			case HTTP_REQUEST_BODY_CSTRING:
				curl_easy_setopt(request->ch, CURLOPT_POSTFIELDS, request->body->data);
				curl_easy_setopt(request->ch, CURLOPT_POSTFIELDSIZE, request->body->size);
			break;

			case HTTP_REQUEST_BODY_CURLPOST:
				curl_easy_setopt(request->ch, CURLOPT_HTTPPOST, (struct curl_httppost *) request->body->data);
			break;

			case HTTP_REQUEST_BODY_UPLOADFILE:
				curl_easy_setopt(request->ch, CURLOPT_IOCTLDATA, request);
				curl_easy_setopt(request->ch, CURLOPT_READDATA, request);
				curl_easy_setopt(request->ch, CURLOPT_INFILESIZE, request->body->size);
			break;

			default:
				/* shouldn't ever happen */
				http_error_ex(HE_ERROR, 0, "Unknown request body type: %d (%s)", request->body->type, request->url);
				return FAILURE;
			break;
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ void http_request_exec(http_request *) */
PHP_HTTP_API void _http_request_exec(http_request *request)
{
	CURLcode result;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	if (CURLE_OK != (result = curl_easy_perform(request->ch))) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(result), request->_error, request->url);
	}
}
/* }}} */

/* {{{ void http_request_info(http_request *, HashTable *) */
PHP_HTTP_API void _http_request_info(http_request *request, HashTable *info)
{
	zval array;
	INIT_ZARR(array, info);

	HTTP_CURL_INFO(EFFECTIVE_URL);
	HTTP_CURL_INFO(RESPONSE_CODE);
	HTTP_CURL_INFO_EX(HTTP_CONNECTCODE, connect_code);
	HTTP_CURL_INFO(FILETIME);
	HTTP_CURL_INFO(TOTAL_TIME);
	HTTP_CURL_INFO(NAMELOOKUP_TIME);
	HTTP_CURL_INFO(CONNECT_TIME);
	HTTP_CURL_INFO(PRETRANSFER_TIME);
	HTTP_CURL_INFO(STARTTRANSFER_TIME);
	HTTP_CURL_INFO(REDIRECT_TIME);
	HTTP_CURL_INFO(REDIRECT_COUNT);
	HTTP_CURL_INFO(SIZE_UPLOAD);
	HTTP_CURL_INFO(SIZE_DOWNLOAD);
	HTTP_CURL_INFO(SPEED_DOWNLOAD);
	HTTP_CURL_INFO(SPEED_UPLOAD);
	HTTP_CURL_INFO(HEADER_SIZE);
	HTTP_CURL_INFO(REQUEST_SIZE);
	HTTP_CURL_INFO(SSL_VERIFYRESULT);
	HTTP_CURL_INFO(SSL_ENGINES);
	HTTP_CURL_INFO(CONTENT_LENGTH_DOWNLOAD);
	HTTP_CURL_INFO(CONTENT_LENGTH_UPLOAD);
	HTTP_CURL_INFO(CONTENT_TYPE);
	/*HTTP_CURL_INFO(PRIVATE);*/
	HTTP_CURL_INFO(HTTPAUTH_AVAIL);
	HTTP_CURL_INFO(PROXYAUTH_AVAIL);
	/*HTTP_CURL_INFO(OS_ERRNO);*/
	HTTP_CURL_INFO(NUM_CONNECTS);
#if LIBCURL_VERSION_NUM >= 0x070e01
	HTTP_CURL_INFO_EX(COOKIELIST, cookies);
#endif
}
/* }}} */

/* {{{ static size_t http_curl_read_callback(void *, size_t, size_t, void *) */
static size_t http_curl_read_callback(void *data, size_t len, size_t n, void *ctx)
{
	http_request *request = (http_request *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);

	if (request->body == NULL || request->body->type != HTTP_REQUEST_BODY_UPLOADFILE) {
		return 0;
	}
	return php_stream_read((php_stream *) request->body->data, data, len * n);
}
/* }}} */

/* {{{ static int http_curl_progress_callback(void *, double, double, double, double) */
static int http_curl_progress_callback(void *ctx, double dltotal, double dlnow, double ultotal, double ulnow)
{
	int rc;
	zval *params_pass[4], params_local[4], retval;
	http_request *request = (http_request *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);

	params_pass[0] = &params_local[0];
	params_pass[1] = &params_local[1];
	params_pass[2] = &params_local[2];
	params_pass[3] = &params_local[3];

	INIT_PZVAL(params_pass[0]);
	INIT_PZVAL(params_pass[1]);
	INIT_PZVAL(params_pass[2]);
	INIT_PZVAL(params_pass[3]);
	ZVAL_DOUBLE(params_pass[0], dltotal);
	ZVAL_DOUBLE(params_pass[1], dlnow);
	ZVAL_DOUBLE(params_pass[2], ultotal);
	ZVAL_DOUBLE(params_pass[3], ulnow);

	INIT_PZVAL(&retval);
	ZVAL_NULL(&retval);
	rc = call_user_function(EG(function_table), NULL, request->_progress_callback, &retval, 4, params_pass TSRMLS_CC);
	zval_dtor(&retval);
	return rc;
}
/* }}} */

/* {{{ static curlioerr http_curl_ioctl_callback(CURL *, curliocmd, void *) */
static curlioerr http_curl_ioctl_callback(CURL *ch, curliocmd cmd, void *ctx)
{
	http_request *request = (http_request *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	if (cmd != CURLIOCMD_RESTARTREAD) {
		return CURLIOE_UNKNOWNCMD;
	}
	if (	request->body == NULL || 
			request->body->type != HTTP_REQUEST_BODY_UPLOADFILE ||
			SUCCESS != php_stream_rewind((php_stream *) request->body->data)) {
		return CURLIOE_FAILRESTART;
	}
	return CURLIOE_OK;
}
/* }}} */

/* {{{ static int http_curl_raw_callback(CURL *, curl_infotype, char *, size_t, void *) */
static int http_curl_raw_callback(CURL *ch, curl_infotype type, char *data, size_t length, void *ctx)
{
	http_request *request = (http_request *) ctx;

	switch (type)
	{
		case CURLINFO_DATA_IN:
			if (request->conv.last_type == CURLINFO_HEADER_IN) {
				phpstr_appends(&request->conv.response, HTTP_CRLF);
			}
		case CURLINFO_HEADER_IN:
			phpstr_append(&request->conv.response, data, length);
		break;
		case CURLINFO_DATA_OUT:
			if (request->conv.last_type == CURLINFO_HEADER_OUT) {
				phpstr_appends(&request->conv.request, HTTP_CRLF);
			}
		case CURLINFO_HEADER_OUT:
			phpstr_append(&request->conv.request, data, length);
		break;
		default:
#if 0
			fprintf(stderr, "## ", type);
			if (!type) {
				fprintf(stderr, "%s", data);
			} else {
				ulong i;
				for (i = 1; i <= length; ++i) {
					fprintf(stderr, "%02X ", data[i-1] & 0xFF);
					if (!(i % 20)) {
						fprintf(stderr, "\n## ");
					}
				}
				fprintf(stderr, "\n");
			}
			if (data[length-1] != 0xa) {
				fprintf(stderr, "\n");
			}
#endif
		break;
	}

	if (type) {
		request->conv.last_type = type;
	}
	return 0;
}
/* }}} */

/* {{{ static inline zval *http_request_option(http_request *, HashTable *, char *, size_t, int) */
static inline zval *_http_request_option_ex(http_request *r, HashTable *options, char *key, size_t keylen, int type TSRMLS_DC)
{
	zval **zoption;
#ifdef ZEND_ENGINE_2
	ulong h = zend_get_hash_value(key, keylen);
#endif
	
	if (!options || 
#ifdef ZEND_ENGINE_2
			(SUCCESS != zend_hash_quick_find(options, key, keylen, h, (void **) &zoption))
#else
			(SUCCESS != zend_hash_find(options, key, keylen, (void **) &zoption))
#endif
	) {
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
			case IS_OBJECT:	convert_to_object_ex(zoption);	break;
			default:
			break;
		}
	}
	
	ZVAL_ADDREF(*zoption);
#ifdef ZEND_ENGINE_2
	_zend_hash_quick_add_or_update(&r->_cache.options, key, keylen, h, zoption, sizeof(zval *), NULL, 
		zend_hash_quick_exists(&r->_cache.options, key, keylen, h)?HASH_UPDATE:HASH_ADD ZEND_FILE_LINE_CC);
#else
	zend_hash_add_or_update(&r->_cache.options, key, keylen, zoption, sizeof(zval *), NULL,
		zend_hash_exists(&r->_cache.options, key, keylen)?HASH_UPDATE:HASH_ADD);
#endif
	
	return *zoption;
}
/* }}} */

#ifdef HTTP_NEED_OPENSSL_TSL
/* {{{ */
static MUTEX_T *http_openssl_tsl = NULL;

static void http_ssl_lock(int mode, int n, const char * file, int line)
{
	if (mode & CRYPTO_LOCK) {
		tsrm_mutex_lock(http_openssl_tsl[n]);
	} else {
		tsrm_mutex_unlock(http_openssl_tsl[n]);
	}
}

static ulong http_ssl_id(void)
{
	return (ulong) tsrm_thread_id();
}

static inline void http_ssl_init(void)
{
	int i, c = CRYPTO_num_locks();
	
	http_openssl_tsl = malloc(c * sizeof(MUTEX_T));
	
	for (i = 0; i < c; ++i) {
		http_openssl_tsl[i] = tsrm_mutex_alloc();
	}
	
	CRYPTO_set_id_callback(http_ssl_id);
	CRYPTO_set_locking_callback(http_ssl_lock);
}

static inline void http_ssl_cleanup(void)
{
	if (http_openssl_tsl) {
		int i, c = CRYPTO_num_locks();
		
		CRYPTO_set_id_callback(NULL);
		CRYPTO_set_locking_callback(NULL);
		
		for (i = 0; i < c; ++i) {
			tsrm_mutex_free(http_openssl_tsl[i]);
		}
		
		free(http_openssl_tsl);
		http_openssl_tsl = NULL;
	}
}
#endif /* HTTP_NEED_OPENSSL_TSL */
/* }}} */

#ifdef HTTP_NEED_GNUTLS_TSL
/* {{{ */
static int http_ssl_mutex_create(void **m)
{
	if (*((MUTEX_T *) m) = tsrm_mutex_alloc()) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

static int http_ssl_mutex_destroy(void **m)
{
	tsrm_mutex_free(*((MUTEX_T *) m));
	return SUCCESS;
}

static int http_ssl_mutex_lock(void **m)
{
	return tsrm_mutex_lock(*((MUTEX_T *) m));
}

static int http_ssl_mutex_unlock(void **m)
{
	return tsrm_mutex_unlock(*((MUTEX_T *) m));
}

static struct gcry_thread_cbs http_gnutls_tsl = {
	GCRY_THREAD_OPTIONS_USER,
	NULL,
	http_ssl_mutex_create,
	http_ssl_mutex_destroy,
	http_ssl_mutex_lock,
	http_ssl_mutex_unlock
};

static inline void http_ssl_init(void)
{
	gcry_control(GCRYCTL_SET_THREAD_CBS, &http_gnutls_tsl);
}

static inline void http_ssl_cleanup(void)
{
	return;
}
#endif /* HTTP_NEED_GNUTLS_TSL */
/* }}} */

#endif /* HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

