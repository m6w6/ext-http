/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2010, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_SAPI
#define HTTP_WANT_CURL
#include "php_http.h"

#ifdef HTTP_HAVE_CURL

#include "php_http_api.h"
#include "php_http_persistent_handle_api.h"
#include "php_http_request_api.h"
#include "php_http_url_api.h"

#ifdef ZEND_ENGINE_2
#	include "php_http_request_object.h"
#endif

#include "php_http_request_int.h"

/* {{{ cruft for thread safe SSL crypto locks */
#ifdef HTTP_NEED_OPENSSL_TSL
static MUTEX_T *http_openssl_tsl = NULL;

static void http_openssl_thread_lock(int mode, int n, const char * file, int line)
{
	if (mode & CRYPTO_LOCK) {
		tsrm_mutex_lock(http_openssl_tsl[n]);
	} else {
		tsrm_mutex_unlock(http_openssl_tsl[n]);
	}
}

static ulong http_openssl_thread_id(void)
{
	return (ulong) tsrm_thread_id();
}
#endif
#ifdef HTTP_NEED_GNUTLS_TSL
static int http_gnutls_mutex_create(void **m)
{
	if (*((MUTEX_T *) m) = tsrm_mutex_alloc()) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

static int http_gnutls_mutex_destroy(void **m)
{
	tsrm_mutex_free(*((MUTEX_T *) m));
	return SUCCESS;
}

static int http_gnutls_mutex_lock(void **m)
{
	return tsrm_mutex_lock(*((MUTEX_T *) m));
}

static int http_gnutls_mutex_unlock(void **m)
{
	return tsrm_mutex_unlock(*((MUTEX_T *) m));
}

static struct gcry_thread_cbs http_gnutls_tsl = {
	GCRY_THREAD_OPTION_USER,
	NULL,
	http_gnutls_mutex_create,
	http_gnutls_mutex_destroy,
	http_gnutls_mutex_lock,
	http_gnutls_mutex_unlock
};
#endif
/* }}} */

/* safe curl wrappers */
#define init_curl_storage(ch) \
	{\
		http_request_storage *st = pecalloc(1, sizeof(http_request_storage), 1); \
		curl_easy_setopt(ch, CURLOPT_PRIVATE, st); \
		curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, st->errorbuffer); \
	}

static void *safe_curl_init(void)
{
	CURL *ch;
	
	if ((ch = curl_easy_init())) {
		init_curl_storage(ch);
		return ch;
	}
	return NULL;
}
static void *safe_curl_copy(void *p)
{
	CURL *ch;
	
	if ((ch = curl_easy_duphandle(p))) {
		init_curl_storage(ch);
		return ch;
	}
	return NULL;
}
static void safe_curl_dtor(void *p) {
	http_request_storage *st = http_request_storage_get(p);
	
	curl_easy_cleanup(p);
	
	if (st) {
		if (st->url) {
			pefree(st->url, 1);
		}
		if (st->cookiestore) {
			pefree(st->cookiestore, 1);
		}
		pefree(st, 1);
	}
}
/* }}} */

/* {{{ MINIT */
PHP_MINIT_FUNCTION(http_request)
{
#ifdef HTTP_NEED_OPENSSL_TSL
	/* mod_ssl, libpq or ext/curl might already have set thread lock callbacks */
	if (!CRYPTO_get_id_callback()) {
		int i, c = CRYPTO_num_locks();
		
		http_openssl_tsl = malloc(c * sizeof(MUTEX_T));
		
		for (i = 0; i < c; ++i) {
			http_openssl_tsl[i] = tsrm_mutex_alloc();
		}
		
		CRYPTO_set_id_callback(http_openssl_thread_id);
		CRYPTO_set_locking_callback(http_openssl_thread_lock);
	}
#endif
#ifdef HTTP_NEED_GNUTLS_TSL
	gcry_control(GCRYCTL_SET_THREAD_CBS, &http_gnutls_tsl);
#endif

	if (CURLE_OK != curl_global_init(CURL_GLOBAL_ALL)) {
		return FAILURE;
	}
	
	if (SUCCESS != http_persistent_handle_provide("http_request", safe_curl_init, safe_curl_dtor, safe_curl_copy)) {
		return FAILURE;
	}
	
	HTTP_LONG_CONSTANT("HTTP_AUTH_BASIC", CURLAUTH_BASIC);
	HTTP_LONG_CONSTANT("HTTP_AUTH_DIGEST", CURLAUTH_DIGEST);
#if HTTP_CURL_VERSION(7,19,3)
	HTTP_LONG_CONSTANT("HTTP_AUTH_DIGEST_IE", CURLAUTH_DIGEST_IE);
#endif
	HTTP_LONG_CONSTANT("HTTP_AUTH_NTLM", CURLAUTH_NTLM);
	HTTP_LONG_CONSTANT("HTTP_AUTH_GSSNEG", CURLAUTH_GSSNEGOTIATE);
	HTTP_LONG_CONSTANT("HTTP_AUTH_ANY", CURLAUTH_ANY);
	
	HTTP_LONG_CONSTANT("HTTP_VERSION_NONE", CURL_HTTP_VERSION_NONE); /* to be removed */
	HTTP_LONG_CONSTANT("HTTP_VERSION_1_0", CURL_HTTP_VERSION_1_0);
	HTTP_LONG_CONSTANT("HTTP_VERSION_1_1", CURL_HTTP_VERSION_1_1);
	HTTP_LONG_CONSTANT("HTTP_VERSION_ANY", CURL_HTTP_VERSION_NONE);
	
	HTTP_LONG_CONSTANT("HTTP_SSL_VERSION_TLSv1", CURL_SSLVERSION_TLSv1);
	HTTP_LONG_CONSTANT("HTTP_SSL_VERSION_SSLv2", CURL_SSLVERSION_SSLv2);
	HTTP_LONG_CONSTANT("HTTP_SSL_VERSION_SSLv3", CURL_SSLVERSION_SSLv3);
	HTTP_LONG_CONSTANT("HTTP_SSL_VERSION_ANY", CURL_SSLVERSION_DEFAULT);
	
	HTTP_LONG_CONSTANT("HTTP_IPRESOLVE_V4", CURL_IPRESOLVE_V4);
	HTTP_LONG_CONSTANT("HTTP_IPRESOLVE_V6", CURL_IPRESOLVE_V6);
	HTTP_LONG_CONSTANT("HTTP_IPRESOLVE_ANY", CURL_IPRESOLVE_WHATEVER);

#if HTTP_CURL_VERSION(7,15,2)
	HTTP_LONG_CONSTANT("HTTP_PROXY_SOCKS4", CURLPROXY_SOCKS4);
#endif
#if HTTP_CURL_VERSION(7,18,0)
	HTTP_LONG_CONSTANT("HTTP_PROXY_SOCKS4A", CURLPROXY_SOCKS4A);
	HTTP_LONG_CONSTANT("HTTP_PROXY_SOCKS5_HOSTNAME", CURLPROXY_SOCKS5_HOSTNAME);
#endif
	HTTP_LONG_CONSTANT("HTTP_PROXY_SOCKS5", CURLPROXY_SOCKS5);
	HTTP_LONG_CONSTANT("HTTP_PROXY_HTTP", CURLPROXY_HTTP);
#if HTTP_CURL_VERSION(7,19,4)
	HTTP_LONG_CONSTANT("HTTP_PROXY_HTTP_1_0", CURLPROXY_HTTP_1_0);
#endif

#if HTTP_CURL_VERSION(7,19,1)
	HTTP_LONG_CONSTANT("HTTP_POSTREDIR_301", CURL_REDIR_POST_301);
	HTTP_LONG_CONSTANT("HTTP_POSTREDIR_302", CURL_REDIR_POST_302);
	HTTP_LONG_CONSTANT("HTTP_POSTREDIR_ALL", CURL_REDIR_POST_ALL);
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ MSHUTDOWN */
PHP_MSHUTDOWN_FUNCTION(http_request)
{
	curl_global_cleanup();
#ifdef HTTP_NEED_OPENSSL_TSL
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
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ forward declarations */
#define http_request_option(r, o, k, t) _http_request_option_ex((r), (o), (k), sizeof(k), (t) TSRMLS_CC)
#define http_request_option_ex(r, o, k, l, t) _http_request_option_ex((r), (o), (k), (l), (t) TSRMLS_CC)
static inline zval *_http_request_option_ex(http_request *request, HashTable *options, char *key, size_t keylen, int type TSRMLS_DC);
#define http_request_option_cache(r, k, z) _http_request_option_cache_ex((r), (k), sizeof(k), 0, (z) TSRMLS_CC)
#define http_request_option_cache_ex(r, k, kl, h, z) _http_request_option_cache_ex((r), (k), (kl), (h), (z) TSRMLS_CC)
static inline zval *_http_request_option_cache_ex(http_request *r, char *key, size_t keylen, ulong h, zval *opt TSRMLS_DC);

#define http_request_cookies_enabled(r) _http_request_cookies_enabled((r))
static inline int _http_request_cookies_enabled(http_request *r);

static size_t http_curl_read_callback(void *, size_t, size_t, void *);
static int http_curl_progress_callback(void *, double, double, double, double);
static int http_curl_raw_callback(CURL *, curl_infotype, char *, size_t, void *);
static int http_curl_dummy_callback(char *data, size_t n, size_t l, void *s) { return n*l; }
static curlioerr http_curl_ioctl_callback(CURL *, curliocmd, void *);
/* }}} */

/* {{{ CURL *http_curl_init(http_request *) */
PHP_HTTP_API CURL * _http_curl_init_ex(CURL *ch, http_request *request TSRMLS_DC)
{
	if (ch || (SUCCESS == http_persistent_handle_acquire("http_request", &ch))) {
#if defined(ZTS)
		curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1L);
#endif
		curl_easy_setopt(ch, CURLOPT_HEADER, 0L);
		curl_easy_setopt(ch, CURLOPT_FILETIME, 1L);
		curl_easy_setopt(ch, CURLOPT_AUTOREFERER, 1L);
		curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(ch, CURLOPT_HEADERFUNCTION, NULL);
		curl_easy_setopt(ch, CURLOPT_DEBUGFUNCTION, http_curl_raw_callback);
		curl_easy_setopt(ch, CURLOPT_READFUNCTION, http_curl_read_callback);
		curl_easy_setopt(ch, CURLOPT_IOCTLFUNCTION, http_curl_ioctl_callback);
		curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, http_curl_dummy_callback);
		
		/* set context */
		if (request) {
			curl_easy_setopt(ch, CURLOPT_DEBUGDATA, request);
			
			/* attach curl handle */
			request->ch = ch;
			/* set defaults (also in http_request_reset()) */
			http_request_defaults(request);
		}
	}
	
	return ch;
}
/* }}} */

/* {{{ CURL *http_curl_copy(CURL *) */
PHP_HTTP_API CURL *_http_curl_copy(CURL *ch TSRMLS_DC)
{
	CURL *copy;
	
	if (SUCCESS == http_persistent_handle_accrete("http_request", ch, &copy)) {
		return copy;
	}
	return NULL;
}
/* }}} */

/* {{{ void http_curl_free(CURL **) */
PHP_HTTP_API void _http_curl_free(CURL **ch TSRMLS_DC)
{
	if (*ch) {
		curl_easy_setopt(*ch, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(*ch, CURLOPT_PROGRESSFUNCTION, NULL);
		curl_easy_setopt(*ch, CURLOPT_VERBOSE, 0L);
		curl_easy_setopt(*ch, CURLOPT_DEBUGFUNCTION, NULL);
		
		http_persistent_handle_release("http_request", ch);
	}
}
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
	r->url = (url) ? http_absolute_url(url) : NULL;
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
	
	http_request_reset(request);
	http_curl_free(&request->ch);
	
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
	http_request_defaults(request);
	
	if (request->ch) {
		http_request_storage *st = http_request_storage_get(request->ch);
		
		if (st) {
			if (st->url) {
				pefree(st->url, 1);
				st->url = NULL;
			}
			if (st->cookiestore) {
				pefree(st->cookiestore, 1);
				st->cookiestore = NULL;
			}
			st->errorbuffer[0] = '\0';
		}
	}
}
/* }}} */

/* {{{ STATUS http_request_enable_cookies(http_request *) */
PHP_HTTP_API STATUS _http_request_enable_cookies(http_request *request)
{
	int initialized = 1;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	HTTP_CHECK_CURL_INIT(request->ch, http_curl_init_ex(request->ch, request), initialized = 0);
	if (initialized && (http_request_cookies_enabled(request) || (CURLE_OK == curl_easy_setopt(request->ch, CURLOPT_COOKIEFILE, "")))) {
		return SUCCESS;
	}
	http_error(HE_WARNING, HTTP_E_REQUEST, "Could not enable cookies for this session");
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_request_reset_cookies(http_request *, int) */
PHP_HTTP_API STATUS _http_request_reset_cookies(http_request *request, int session_only)
{
	int initialized = 1;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	HTTP_CHECK_CURL_INIT(request->ch, http_curl_init_ex(request->ch, request), initialized = 0);
	if (initialized) {
		if (!http_request_cookies_enabled(request)) {
			if (SUCCESS != http_request_enable_cookies(request)) {
				return FAILURE;
			}
		}
		if (session_only) {
#if HTTP_CURL_VERSION(7,15,4)
			if (CURLE_OK == curl_easy_setopt(request->ch, CURLOPT_COOKIELIST, "SESS")) {
				return SUCCESS;
			}
#else
			http_error(HE_WARNING, HTTP_E_REQUEST, "Could not reset session cookies (need libcurl >= v7.15.4)");
#endif
		} else {
#if HTTP_CURL_VERSION(7,14,1)
			if (CURLE_OK == curl_easy_setopt(request->ch, CURLOPT_COOKIELIST, "ALL")) {
				return SUCCESS;
			}
#else
			http_error(HE_WARNING, HTTP_E_REQUEST, "Could not reset cookies (need libcurl >= v7.14.1)");
#endif
		}
	}
	return FAILURE;
}
/* }}} */

PHP_HTTP_API STATUS _http_request_flush_cookies(http_request *request)
{
	int initialized = 1;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	HTTP_CHECK_CURL_INIT(request->ch, http_curl_init_ex(request->ch, request), initialized = 0);
	if (initialized) {
		if (!http_request_cookies_enabled(request)) {
			return FAILURE;
		}
#if HTTP_CURL_VERSION(7,17,1)
		if (CURLE_OK == curl_easy_setopt(request->ch, CURLOPT_COOKIELIST, "FLUSH")) {
			return SUCCESS;
		}
#else
		http_error(HE_WARNING, HTTP_E_REQUEST, "Could not flush cookies (need libcurl >= v7.17.1)");
#endif
	}
	return FAILURE;
}

/* {{{ void http_request_defaults(http_request *) */
PHP_HTTP_API void _http_request_defaults(http_request *request)
{
	if (request->ch) {
		HTTP_CURL_OPT(CURLOPT_PROGRESSFUNCTION, NULL);
		HTTP_CURL_OPT(CURLOPT_URL, NULL);
		HTTP_CURL_OPT(CURLOPT_NOPROGRESS, 1L);
#if HTTP_CURL_VERSION(7,19,4)
		HTTP_CURL_OPT(CURLOPT_NOPROXY, NULL);
#endif
		HTTP_CURL_OPT(CURLOPT_PROXY, NULL);
		HTTP_CURL_OPT(CURLOPT_PROXYPORT, 0L);
		HTTP_CURL_OPT(CURLOPT_PROXYTYPE, 0L);
		/* libcurl < 7.19.6 does not clear auth info with USERPWD set to NULL */
#if HTTP_CURL_VERSION(7,19,1)		
		HTTP_CURL_OPT(CURLOPT_PROXYUSERNAME, NULL);
		HTTP_CURL_OPT(CURLOPT_PROXYPASSWORD, NULL);
#endif
		HTTP_CURL_OPT(CURLOPT_PROXYAUTH, 0L);
		HTTP_CURL_OPT(CURLOPT_HTTPPROXYTUNNEL, 0L);
		HTTP_CURL_OPT(CURLOPT_DNS_CACHE_TIMEOUT, 60L);
		HTTP_CURL_OPT(CURLOPT_IPRESOLVE, 0);
		HTTP_CURL_OPT(CURLOPT_LOW_SPEED_LIMIT, 0L);
		HTTP_CURL_OPT(CURLOPT_LOW_SPEED_TIME, 0L);
#if HTTP_CURL_VERSION(7,15,5)
		/* LFS weirdance
		HTTP_CURL_OPT(CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t) 0);
		HTTP_CURL_OPT(CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t) 0);
		*/
#endif
		/* crashes
		HTTP_CURL_OPT(CURLOPT_MAXCONNECTS, 5L); */
		HTTP_CURL_OPT(CURLOPT_FRESH_CONNECT, 0L);
		HTTP_CURL_OPT(CURLOPT_FORBID_REUSE, 0L);
		HTTP_CURL_OPT(CURLOPT_INTERFACE, NULL);
		HTTP_CURL_OPT(CURLOPT_PORT, 0L);
#if HTTP_CURL_VERSION(7,19,0)
		HTTP_CURL_OPT(CURLOPT_ADDRESS_SCOPE, 0L);
#endif
#if HTTP_CURL_VERSION(7,15,2)
		HTTP_CURL_OPT(CURLOPT_LOCALPORT, 0L);
		HTTP_CURL_OPT(CURLOPT_LOCALPORTRANGE, 0L);
#endif
		/* libcurl < 7.19.6 does not clear auth info with USERPWD set to NULL */
#if HTTP_CURL_VERSION(7,19,1)
		HTTP_CURL_OPT(CURLOPT_USERNAME, NULL);
		HTTP_CURL_OPT(CURLOPT_PASSWORD, NULL);
#endif
		HTTP_CURL_OPT(CURLOPT_HTTPAUTH, 0L);
		HTTP_CURL_OPT(CURLOPT_ENCODING, NULL);
#if HTTP_CURL_VERSION(7,16,2)
		/* we do this ourself anyway */
		HTTP_CURL_OPT(CURLOPT_HTTP_CONTENT_DECODING, 0L);
		HTTP_CURL_OPT(CURLOPT_HTTP_TRANSFER_DECODING, 0L);
#endif
		HTTP_CURL_OPT(CURLOPT_FOLLOWLOCATION, 0L);
#if HTTP_CURL_VERSION(7,19,1)
		HTTP_CURL_OPT(CURLOPT_POSTREDIR, 0L);
#elif HTTP_CURL_VERSION(7,17,1)
		HTTP_CURL_OPT(CURLOPT_POST301, 0L);
#endif
		HTTP_CURL_OPT(CURLOPT_UNRESTRICTED_AUTH, 0L);
		HTTP_CURL_OPT(CURLOPT_REFERER, NULL);
		HTTP_CURL_OPT(CURLOPT_USERAGENT, "PECL::HTTP/" PHP_HTTP_VERSION " (PHP/" PHP_VERSION ")");
		HTTP_CURL_OPT(CURLOPT_HTTPHEADER, NULL);
		HTTP_CURL_OPT(CURLOPT_COOKIE, NULL);
		HTTP_CURL_OPT(CURLOPT_COOKIESESSION, 0L);
		/* these options would enable curl's cookie engine by default which we don't want
		HTTP_CURL_OPT(CURLOPT_COOKIEFILE, NULL);
		HTTP_CURL_OPT(CURLOPT_COOKIEJAR, NULL); */
#if HTTP_CURL_VERSION(7,14,1)
		HTTP_CURL_OPT(CURLOPT_COOKIELIST, NULL);
#endif
		HTTP_CURL_OPT(CURLOPT_RANGE, NULL);
		HTTP_CURL_OPT(CURLOPT_RESUME_FROM, 0L);
		HTTP_CURL_OPT(CURLOPT_MAXFILESIZE, 0L);
		HTTP_CURL_OPT(CURLOPT_TIMECONDITION, 0L);
		HTTP_CURL_OPT(CURLOPT_TIMEVALUE, 0L);
		HTTP_CURL_OPT(CURLOPT_TIMEOUT, 0L);
		HTTP_CURL_OPT(CURLOPT_CONNECTTIMEOUT, 3);
		HTTP_CURL_OPT(CURLOPT_SSLCERT, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLCERTTYPE, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLCERTPASSWD, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLKEY, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLKEYTYPE, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLKEYPASSWD, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLENGINE, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLVERSION, 0L);
		HTTP_CURL_OPT(CURLOPT_SSL_VERIFYPEER, 0L);
		HTTP_CURL_OPT(CURLOPT_SSL_VERIFYHOST, 0L);
		HTTP_CURL_OPT(CURLOPT_SSL_CIPHER_LIST, NULL);
#if HTTP_CURL_VERSION(7,19,0)
		HTTP_CURL_OPT(CURLOPT_ISSUERCERT, NULL);
	#if defined(HTTP_HAVE_OPENSSL)
		HTTP_CURL_OPT(CURLOPT_CRLFILE, NULL);
	#endif
#endif
#if HTTP_CURL_VERSION(7,19,1) && defined(HTTP_HAVE_OPENSSL)
		HTTP_CURL_OPT(CURLOPT_CERTINFO, NULL);
#endif
#ifdef HTTP_CURL_CAINFO
		HTTP_CURL_OPT(CURLOPT_CAINFO, HTTP_CURL_CAINFO);
#else
		HTTP_CURL_OPT(CURLOPT_CAINFO, NULL);
#endif
		HTTP_CURL_OPT(CURLOPT_CAPATH, NULL);
		HTTP_CURL_OPT(CURLOPT_RANDOM_FILE, NULL);
		HTTP_CURL_OPT(CURLOPT_EGDSOCKET, NULL);
		HTTP_CURL_OPT(CURLOPT_POSTFIELDS, NULL);
		HTTP_CURL_OPT(CURLOPT_POSTFIELDSIZE, 0L);
		HTTP_CURL_OPT(CURLOPT_HTTPPOST, NULL);
		HTTP_CURL_OPT(CURLOPT_IOCTLDATA, NULL);
		HTTP_CURL_OPT(CURLOPT_READDATA, NULL);
		HTTP_CURL_OPT(CURLOPT_INFILESIZE, 0L);
		HTTP_CURL_OPT(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);
		HTTP_CURL_OPT(CURLOPT_CUSTOMREQUEST, NULL);
		HTTP_CURL_OPT(CURLOPT_NOBODY, 0L);
		HTTP_CURL_OPT(CURLOPT_POST, 0L);
		HTTP_CURL_OPT(CURLOPT_UPLOAD, 0L);
		HTTP_CURL_OPT(CURLOPT_HTTPGET, 1L);
	}
}
/* }}} */

PHP_HTTP_API void _http_request_set_progress_callback(http_request *request, zval *cb)
{
	if (request->_progress_callback) {
		zval_ptr_dtor(&request->_progress_callback);
	}
	if ((request->_progress_callback = cb)) {
		ZVAL_ADDREF(cb);
		HTTP_CURL_OPT(CURLOPT_NOPROGRESS, 0);
		HTTP_CURL_OPT(CURLOPT_PROGRESSDATA, request);
		HTTP_CURL_OPT(CURLOPT_PROGRESSFUNCTION, http_curl_progress_callback);
	} else {
		HTTP_CURL_OPT(CURLOPT_NOPROGRESS, 1);
		HTTP_CURL_OPT(CURLOPT_PROGRESSDATA, NULL);
		HTTP_CURL_OPT(CURLOPT_PROGRESSFUNCTION, NULL);
	}
}

/* {{{ STATUS http_request_prepare(http_request *, HashTable *) */
PHP_HTTP_API STATUS _http_request_prepare(http_request *request, HashTable *options)
{
	zval *zoption;
	zend_bool range_req = 0;
	http_request_storage *storage;

	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	HTTP_CHECK_CURL_INIT(request->ch, http_curl_init(request), return FAILURE);

	if (!request->url) {
		return FAILURE;
	}
	if (!(storage = http_request_storage_get(request->ch))) {
		return FAILURE;
	}
	storage->errorbuffer[0] = '\0';
	/* set options */
	if (storage->url) {
		pefree(storage->url, 1);
	}
	storage->url = pestrdup(request->url, 1);
	HTTP_CURL_OPT(CURLOPT_URL, storage->url);

	/* progress callback */
	if ((zoption = http_request_option(request, options, "onprogress", -1))) {
		http_request_set_progress_callback(request, zoption);
	}

	/* proxy */
	if ((zoption = http_request_option(request, options, "proxyhost", IS_STRING))) {
		HTTP_CURL_OPT(CURLOPT_PROXY, Z_STRVAL_P(zoption));
		/* type */
		if ((zoption = http_request_option(request, options, "proxytype", IS_LONG))) {
			HTTP_CURL_OPT(CURLOPT_PROXYTYPE, Z_LVAL_P(zoption));
		}
		/* port */
		if ((zoption = http_request_option(request, options, "proxyport", IS_LONG))) {
			HTTP_CURL_OPT(CURLOPT_PROXYPORT, Z_LVAL_P(zoption));
		}
		/* user:pass */
		if ((zoption = http_request_option(request, options, "proxyauth", IS_STRING)) && Z_STRLEN_P(zoption)) {
			HTTP_CURL_OPT(CURLOPT_PROXYUSERPWD, Z_STRVAL_P(zoption));
		}
		/* auth method */
		if ((zoption = http_request_option(request, options, "proxyauthtype", IS_LONG))) {
			HTTP_CURL_OPT(CURLOPT_PROXYAUTH, Z_LVAL_P(zoption));
		}
		/* tunnel */
		if ((zoption = http_request_option(request, options, "proxytunnel", IS_BOOL)) && Z_BVAL_P(zoption)) {
			HTTP_CURL_OPT(CURLOPT_HTTPPROXYTUNNEL, 1L);
		}
	}
#if HTTP_CURL_VERSION(7,19,4)
	if ((zoption = http_request_option(request, options, "noproxy", IS_STRING))) {
		HTTP_CURL_OPT(CURLOPT_NOPROXY, Z_STRVAL_P(zoption));
	}
#endif

	/* dns */
	if ((zoption = http_request_option(request, options, "dns_cache_timeout", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_DNS_CACHE_TIMEOUT, Z_LVAL_P(zoption));
	}
	if ((zoption = http_request_option(request, options, "ipresolve", IS_LONG)) && Z_LVAL_P(zoption)) {
		HTTP_CURL_OPT(CURLOPT_IPRESOLVE, Z_LVAL_P(zoption));
	}
	
	/* limits */
	if ((zoption = http_request_option(request, options, "low_speed_limit", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_LOW_SPEED_LIMIT, Z_LVAL_P(zoption));
	}
	if ((zoption = http_request_option(request, options, "low_speed_time", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_LOW_SPEED_TIME, Z_LVAL_P(zoption));
	}
#if HTTP_CURL_VERSION(7,15,5)
	/* LSF weirdance
	if ((zoption = http_request_option(request, options, "max_send_speed", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t) Z_LVAL_P(zoption));
	}
	if ((zoption = http_request_option(request, options, "max_recv_speed", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t) Z_LVAL_P(zoption));
	}
	*/
#endif
	/* crashes
	if ((zoption = http_request_option(request, options, "maxconnects", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_MAXCONNECTS, Z_LVAL_P(zoption));
	} */
	if ((zoption = http_request_option(request, options, "fresh_connect", IS_BOOL)) && Z_BVAL_P(zoption)) {
		HTTP_CURL_OPT(CURLOPT_FRESH_CONNECT, 1L);
	}
	if ((zoption = http_request_option(request, options, "forbid_reuse", IS_BOOL)) && Z_BVAL_P(zoption)) {
		HTTP_CURL_OPT(CURLOPT_FORBID_REUSE, 1L);
	}
	
	/* outgoing interface */
	if ((zoption = http_request_option(request, options, "interface", IS_STRING))) {
		HTTP_CURL_OPT(CURLOPT_INTERFACE, Z_STRVAL_P(zoption));
		
#if HTTP_CURL_VERSION(7,15,2)
		if ((zoption = http_request_option(request, options, "portrange", IS_ARRAY))) {
			zval **prs, **pre;
			
			zend_hash_internal_pointer_reset(Z_ARRVAL_P(zoption));
			if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void *) &prs)) {
				zend_hash_move_forward(Z_ARRVAL_P(zoption));
				if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void *) &pre)) {
					zval *prs_cpy = http_zsep(IS_LONG, *prs);
					zval *pre_cpy = http_zsep(IS_LONG, *pre);
					
					if (Z_LVAL_P(prs_cpy) && Z_LVAL_P(pre_cpy)) {
						HTTP_CURL_OPT(CURLOPT_LOCALPORT, MIN(Z_LVAL_P(prs_cpy), Z_LVAL_P(pre_cpy)));
						HTTP_CURL_OPT(CURLOPT_LOCALPORTRANGE, labs(Z_LVAL_P(prs_cpy)-Z_LVAL_P(pre_cpy))+1L);
					}
					zval_ptr_dtor(&prs_cpy);
					zval_ptr_dtor(&pre_cpy);
				}
			}
		}
#endif
	}

	/* another port */
	if ((zoption = http_request_option(request, options, "port", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_PORT, Z_LVAL_P(zoption));
	}
	
	/* RFC4007 zone_id */
#if HTTP_CURL_VERSION(7,19,0)
	if ((zoption = http_request_option(request, options, "address_scope", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_ADDRESS_SCOPE, Z_LVAL_P(zoption));
	}
#endif

	/* auth */
	if ((zoption = http_request_option(request, options, "httpauth", IS_STRING)) && Z_STRLEN_P(zoption)) {
		HTTP_CURL_OPT(CURLOPT_USERPWD, Z_STRVAL_P(zoption));
	}
	if ((zoption = http_request_option(request, options, "httpauthtype", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_HTTPAUTH, Z_LVAL_P(zoption));
	}

	/* redirects, defaults to 0 */
	if ((zoption = http_request_option(request, options, "redirect", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_FOLLOWLOCATION, Z_LVAL_P(zoption) ? 1L : 0L);
		HTTP_CURL_OPT(CURLOPT_MAXREDIRS, Z_LVAL_P(zoption));
		if ((zoption = http_request_option(request, options, "unrestrictedauth", IS_BOOL))) {
			HTTP_CURL_OPT(CURLOPT_UNRESTRICTED_AUTH, Z_LVAL_P(zoption));
		}
#if HTTP_CURL_VERSION(7,17,1)
		if ((zoption = http_request_option(request, options, "postredir", IS_BOOL))) {
#	if HTTP_CURL_VERSION(7,19,1)
			HTTP_CURL_OPT(CURLOPT_POSTREDIR, Z_BVAL_P(zoption) ? 1L : 0L);
#	else
			HTTP_CURL_OPT(CURLOPT_POST301, Z_BVAL_P(zoption) ? 1L : 0L);
#	endif
		}
#endif
	}
	
	/* retries, defaults to 0 */
	if ((zoption = http_request_option(request, options, "retrycount", IS_LONG))) {
		request->_retry.count = Z_LVAL_P(zoption);
		if ((zoption = http_request_option(request, options, "retrydelay", IS_DOUBLE))) {
			request->_retry.delay = Z_DVAL_P(zoption);
		} else {
			request->_retry.delay = 0;
		}
	} else {
		request->_retry.count = 0;
	}

	/* referer */
	if ((zoption = http_request_option(request, options, "referer", IS_STRING)) && Z_STRLEN_P(zoption)) {
		HTTP_CURL_OPT(CURLOPT_REFERER, Z_STRVAL_P(zoption));
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if ((zoption = http_request_option(request, options, "useragent", IS_STRING))) {
		/* allow to send no user agent, not even default one */
		if (Z_STRLEN_P(zoption)) {
			HTTP_CURL_OPT(CURLOPT_USERAGENT, Z_STRVAL_P(zoption));
		} else {
			HTTP_CURL_OPT(CURLOPT_USERAGENT, NULL);
		}
	}

	/* resume */
	if ((zoption = http_request_option(request, options, "resume", IS_LONG)) && (Z_LVAL_P(zoption) > 0)) {
		range_req = 1;
		HTTP_CURL_OPT(CURLOPT_RESUME_FROM, Z_LVAL_P(zoption));
	}
	/* or range of kind array(array(0,499), array(100,1499)) */
	else if ((zoption = http_request_option(request, options, "range", IS_ARRAY)) && zend_hash_num_elements(Z_ARRVAL_P(zoption))) {
		HashPosition pos1, pos2;
		zval **rr, **rb, **re;
		phpstr rs;
		
		phpstr_init(&rs);
		FOREACH_VAL(pos1, zoption, rr) {
			if (Z_TYPE_PP(rr) == IS_ARRAY) {
				zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(rr), &pos2);
				if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(rr), (void *) &rb, &pos2)) {
					zend_hash_move_forward_ex(Z_ARRVAL_PP(rr), &pos2);
					if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(rr), (void *) &re, &pos2)) {
						if (	((Z_TYPE_PP(rb) == IS_LONG) || ((Z_TYPE_PP(rb) == IS_STRING) && is_numeric_string(Z_STRVAL_PP(rb), Z_STRLEN_PP(rb), NULL, NULL, 1))) &&
								((Z_TYPE_PP(re) == IS_LONG) || ((Z_TYPE_PP(re) == IS_STRING) && is_numeric_string(Z_STRVAL_PP(re), Z_STRLEN_PP(re), NULL, NULL, 1)))) {
							zval *rbl = http_zsep(IS_LONG, *rb);
							zval *rel = http_zsep(IS_LONG, *re);
							
							if ((Z_LVAL_P(rbl) >= 0) && (Z_LVAL_P(rel) >= 0)) {
								phpstr_appendf(&rs, "%ld-%ld,", Z_LVAL_P(rbl), Z_LVAL_P(rel));
							}
							zval_ptr_dtor(&rbl);
							zval_ptr_dtor(&rel);
						}
					}
				}
			}
		}
		
		if (PHPSTR_LEN(&rs)) {
			zval *cached_range;
			
			/* ditch last comma */
			PHPSTR_VAL(&rs)[PHPSTR_LEN(&rs)-- -1] = '\0';
			/* cache string */
			MAKE_STD_ZVAL(cached_range);
			ZVAL_STRINGL(cached_range, PHPSTR_VAL(&rs), PHPSTR_LEN(&rs), 0);
			HTTP_CURL_OPT(CURLOPT_RANGE, Z_STRVAL_P(http_request_option_cache(request, "range", cached_range)));
			zval_ptr_dtor(&cached_range);
		}
	}

	/* additional headers, array('name' => 'value') */
	if (request->_cache.headers) {
		curl_slist_free_all(request->_cache.headers);
		request->_cache.headers = NULL;
	}
	if ((zoption = http_request_option(request, options, "headers", IS_ARRAY))) {
		HashKey header_key = initHashKey(0);
		zval **header_val;
		HashPosition pos;
		phpstr header;
		
		phpstr_init(&header);
		FOREACH_KEYVAL(pos, zoption, header_key, header_val) {
			if (header_key.type == HASH_KEY_IS_STRING) {
				zval *header_cpy = http_zsep(IS_STRING, *header_val);
				
				if (!strcasecmp(header_key.str, "range")) {
					range_req = 1;
				}

				phpstr_appendf(&header, "%s: %s", header_key.str, Z_STRVAL_P(header_cpy));
				phpstr_fix(&header);
				request->_cache.headers = curl_slist_append(request->_cache.headers, PHPSTR_VAL(&header));
				phpstr_reset(&header);
				
				zval_ptr_dtor(&header_cpy);
			}
		}
		phpstr_dtor(&header);
	}
	/* etag */
	if ((zoption = http_request_option(request, options, "etag", IS_STRING)) && Z_STRLEN_P(zoption)) {
		zend_bool is_quoted;
		phpstr header;

		phpstr_init(&header);
		phpstr_appendf(&header, "%s: ", range_req?"If-Match":"If-None-Match");
		if ((Z_STRVAL_P(zoption)[0] == '"') && (Z_STRVAL_P(zoption)[Z_STRLEN_P(zoption)-1] == '"')) {
			/* properly quoted etag */
			phpstr_append(&header, Z_STRVAL_P(zoption), Z_STRLEN_P(zoption));
		} else if ((Z_STRVAL_P(zoption)[0] == 'W') && (Z_STRVAL_P(zoption)[1] == '/')) {
			/* weak etag */
			if ((Z_STRLEN_P(zoption) > 3) && (Z_STRVAL_P(zoption)[2] == '"') && (Z_STRVAL_P(zoption)[Z_STRLEN_P(zoption)-1] == '"')) {
				/* quoted */
				phpstr_append(&header, Z_STRVAL_P(zoption), Z_STRLEN_P(zoption));
			} else {
				/* unquoted */
				phpstr_appendf(&header, "W/\"%s\"", Z_STRVAL_P(zoption) + 2);
			}
		} else {
			/* assume unquoted etag */
			phpstr_appendf(&header, "\"%s\"", Z_STRVAL_P(zoption));
		}
		phpstr_fix(&header);

		request->_cache.headers = curl_slist_append(request->_cache.headers, PHPSTR_VAL(&header));
		phpstr_dtor(&header);
	}
	/* compression */
	if ((zoption = http_request_option(request, options, "compress", IS_BOOL)) && Z_LVAL_P(zoption)) {
		request->_cache.headers = curl_slist_append(request->_cache.headers, "Accept-Encoding: gzip;q=1.0,deflate;q=0.5");
	}
	HTTP_CURL_OPT(CURLOPT_HTTPHEADER, request->_cache.headers);

	/* lastmodified */
	if ((zoption = http_request_option(request, options, "lastmodified", IS_LONG))) {
		if (Z_LVAL_P(zoption)) {
			if (Z_LVAL_P(zoption) > 0) {
				HTTP_CURL_OPT(CURLOPT_TIMEVALUE, Z_LVAL_P(zoption));
			} else {
				HTTP_CURL_OPT(CURLOPT_TIMEVALUE, (long) HTTP_G->request.time + Z_LVAL_P(zoption));
			}
			HTTP_CURL_OPT(CURLOPT_TIMECONDITION, (long) (range_req ? CURL_TIMECOND_IFUNMODSINCE : CURL_TIMECOND_IFMODSINCE));
		} else {
			HTTP_CURL_OPT(CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
		}
	}

	/* cookies, array('name' => 'value') */
	if ((zoption = http_request_option(request, options, "cookies", IS_ARRAY))) {
		phpstr_dtor(&request->_cache.cookies);
		if (zend_hash_num_elements(Z_ARRVAL_P(zoption))) {
			zval *urlenc_cookies = NULL;
			/* check whether cookies should not be urlencoded; default is to urlencode them */
			if ((!(urlenc_cookies = http_request_option(request, options, "encodecookies", IS_BOOL))) || Z_BVAL_P(urlenc_cookies)) {
				if (SUCCESS == http_urlencode_hash_recursive(HASH_OF(zoption), &request->_cache.cookies, "; ", lenof("; "), NULL, 0)) {
					phpstr_fix(&request->_cache.cookies);
					HTTP_CURL_OPT(CURLOPT_COOKIE, request->_cache.cookies.data);
				}
			} else {
				HashPosition pos;
				HashKey cookie_key = initHashKey(0);
				zval **cookie_val;
				
				FOREACH_KEYVAL(pos, zoption, cookie_key, cookie_val) {
					if (cookie_key.type == HASH_KEY_IS_STRING) {
						zval *val = http_zsep(IS_STRING, *cookie_val);
						phpstr_appendf(&request->_cache.cookies, "%s=%s; ", cookie_key.str, Z_STRVAL_P(val));
						zval_ptr_dtor(&val);
					}
				}
				
				phpstr_fix(&request->_cache.cookies);
				if (PHPSTR_LEN(&request->_cache.cookies)) {
					HTTP_CURL_OPT(CURLOPT_COOKIE, PHPSTR_VAL(&request->_cache.cookies));
				}
			}
		}
	}

	/* don't load session cookies from cookiestore */
	if ((zoption = http_request_option(request, options, "cookiesession", IS_BOOL)) && Z_BVAL_P(zoption)) {
		HTTP_CURL_OPT(CURLOPT_COOKIESESSION, 1L);
	}

	/* cookiestore, read initial cookies from that file and store cookies back into that file */
	if ((zoption = http_request_option(request, options, "cookiestore", IS_STRING))) {
		if (Z_STRLEN_P(zoption)) {
			HTTP_CHECK_OPEN_BASEDIR(Z_STRVAL_P(zoption), return FAILURE);
		}
		if (storage->cookiestore) {
			pefree(storage->cookiestore, 1);
		}
		storage->cookiestore = pestrndup(Z_STRVAL_P(zoption), Z_STRLEN_P(zoption), 1);
		HTTP_CURL_OPT(CURLOPT_COOKIEFILE, storage->cookiestore);
		HTTP_CURL_OPT(CURLOPT_COOKIEJAR, storage->cookiestore);
	}

	/* maxfilesize */
	if ((zoption = http_request_option(request, options, "maxfilesize", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_MAXFILESIZE, Z_LVAL_P(zoption));
	}

	/* http protocol */
	if ((zoption = http_request_option(request, options, "protocol", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_HTTP_VERSION, Z_LVAL_P(zoption));
	}

#if HTTP_CURL_VERSION(7,16,2)
	/* timeout, defaults to 0 */
	if ((zoption = http_request_option(request, options, "timeout", IS_DOUBLE))) {
		HTTP_CURL_OPT(CURLOPT_TIMEOUT_MS, (long)(Z_DVAL_P(zoption)*1000));
	}
	/* connecttimeout, defaults to 0 */
	if ((zoption = http_request_option(request, options, "connecttimeout", IS_DOUBLE))) {
		HTTP_CURL_OPT(CURLOPT_CONNECTTIMEOUT_MS, (long)(Z_DVAL_P(zoption)*1000));
	}
#else
	/* timeout, defaults to 0 */
	if ((zoption = http_request_option(request, options, "timeout", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_TIMEOUT, Z_LVAL_P(zoption));
	}
	/* connecttimeout, defaults to 0 */
	if ((zoption = http_request_option(request, options, "connecttimeout", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_CONNECTTIMEOUT, Z_LVAL_P(zoption));
	}
#endif

	/* ssl */
	if ((zoption = http_request_option(request, options, "ssl", IS_ARRAY))) {
		HashKey key = initHashKey(0);
		zval **param;
		HashPosition pos;

		FOREACH_KEYVAL(pos, zoption, key, param) {
			if (key.type == HASH_KEY_IS_STRING) {
				HTTP_CURL_OPT_STRING(CURLOPT_SSLCERT, 0, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_SSLCERTTYPE, 0, 0);
				HTTP_CURL_OPT_STRING(CURLOPT_SSLCERTPASSWD, 0, 0);

				HTTP_CURL_OPT_STRING(CURLOPT_SSLKEY, 0, 0);
				HTTP_CURL_OPT_STRING(CURLOPT_SSLKEYTYPE, 0, 0);
				HTTP_CURL_OPT_STRING(CURLOPT_SSLKEYPASSWD, 0, 0);

				HTTP_CURL_OPT_STRING(CURLOPT_SSLENGINE, 0, 0);
				HTTP_CURL_OPT_LONG(CURLOPT_SSLVERSION, 0);

				HTTP_CURL_OPT_LONG(CURLOPT_SSL_VERIFYPEER, 1);
				HTTP_CURL_OPT_LONG(CURLOPT_SSL_VERIFYHOST, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_SSL_CIPHER_LIST, 1, 0);

				HTTP_CURL_OPT_STRING(CURLOPT_CAINFO, -3, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_CAPATH, -3, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_RANDOM_FILE, -3, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_EGDSOCKET, -3, 1);
#if HTTP_CURL_VERSION(7,19,0)
				HTTP_CURL_OPT_STRING(CURLOPT_ISSUERCERT, -3, 1);
	#if defined(HTTP_HAVE_OPENSSL)
				HTTP_CURL_OPT_STRING(CURLOPT_CRLFILE, -3, 1);
	#endif
#endif
#if HTTP_CURL_VERSION(7,19,1) && defined(HTTP_HAVE_OPENSSL)
				HTTP_CURL_OPT_LONG(CURLOPT_CERTINFO, -3);
#endif
			}
		}
	}

	/* request method */
	switch (request->meth) {
		case HTTP_GET:
			HTTP_CURL_OPT(CURLOPT_HTTPGET, 1L);
			break;

		case HTTP_HEAD:
			HTTP_CURL_OPT(CURLOPT_NOBODY, 1L);
			break;

		case HTTP_POST:
			HTTP_CURL_OPT(CURLOPT_POST, 1L);
			break;

		case HTTP_PUT:
			HTTP_CURL_OPT(CURLOPT_UPLOAD, 1L);
			break;

		default:
			if (http_request_method_exists(0, request->meth, NULL)) {
				HTTP_CURL_OPT(CURLOPT_CUSTOMREQUEST, http_request_method_name(request->meth));
			} else {
				http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Unsupported request method: %d (%s)", request->meth, request->url);
				return FAILURE;
			}
			break;
	}

	/* attach request body */
	if (request->body && (request->meth != HTTP_GET) && (request->meth != HTTP_HEAD) && (request->meth != HTTP_OPTIONS)) {
		switch (request->body->type) {
			case HTTP_REQUEST_BODY_EMPTY:
				/* nothing */
				break;
			
			case HTTP_REQUEST_BODY_CURLPOST:
				HTTP_CURL_OPT(CURLOPT_HTTPPOST, (struct curl_httppost *) request->body->data);
				break;

			case HTTP_REQUEST_BODY_CSTRING:
				if (request->meth != HTTP_PUT) {
					HTTP_CURL_OPT(CURLOPT_POSTFIELDS, request->body->data);
					HTTP_CURL_OPT(CURLOPT_POSTFIELDSIZE, request->body->size);
					break;
				}
				/* fallthrough, PUT/UPLOAD _needs_ READDATA */
			case HTTP_REQUEST_BODY_UPLOADFILE:
				HTTP_CURL_OPT(CURLOPT_IOCTLDATA, request);
				HTTP_CURL_OPT(CURLOPT_READDATA, request);
				HTTP_CURL_OPT(CURLOPT_INFILESIZE, request->body->size);
				break;

			default:
				/* shouldn't ever happen */
				http_error_ex(HE_ERROR, 0, "Unknown request body type: %d (%s)", request->body->type, request->url);
				return FAILURE;
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ void http_request_exec(http_request *) */
PHP_HTTP_API void _http_request_exec(http_request *request)
{
	uint tries = 0;
	CURLcode result;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
retry:
	if (CURLE_OK != (result = curl_easy_perform(request->ch))) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(result), http_request_storage_get(request->ch)->errorbuffer, request->url);
#ifdef ZEND_ENGINE_2
		if ((HTTP_G->only_exceptions || GLOBAL_ERROR_HANDLING == EH_THROW) && EG(exception)) {
			add_property_long(EG(exception), "curlCode", result);
		}
#endif
		
		if (request->_retry.count > tries++) {
			switch (result) {
				case CURLE_COULDNT_RESOLVE_PROXY:
				case CURLE_COULDNT_RESOLVE_HOST:
				case CURLE_COULDNT_CONNECT:
				case CURLE_WRITE_ERROR:
				case CURLE_READ_ERROR:
				case CURLE_OPERATION_TIMEDOUT:
				case CURLE_SSL_CONNECT_ERROR:
				case CURLE_GOT_NOTHING:
				case CURLE_SSL_ENGINE_SETFAILED:
				case CURLE_SEND_ERROR:
				case CURLE_RECV_ERROR:
				case CURLE_SSL_ENGINE_INITFAILED:
				case CURLE_LOGIN_DENIED:
					if (request->_retry.delay >= HTTP_DIFFSEC) {
						http_sleep(request->_retry.delay);
					}
					goto retry;
				default:
					break;
			}
		}
	}
}
/* }}} */

/* {{{ static size_t http_curl_read_callback(void *, size_t, size_t, void *) */
static size_t http_curl_read_callback(void *data, size_t len, size_t n, void *ctx)
{
	http_request *request = (http_request *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	if (request->body) {
		switch (request->body->type) {
			case HTTP_REQUEST_BODY_CSTRING:
			{
				size_t out = MIN(len * n, request->body->size - request->body->priv);
				
				if (out) {
					memcpy(data, ((char *) request->body->data) + request->body->priv, out);
					request->body->priv += out;
					return out;
				}
				break;
			}
			
			case HTTP_REQUEST_BODY_UPLOADFILE:
				return php_stream_read((php_stream *) request->body->data, data, len * n);
		}
	}
	return 0;
}
/* }}} */

/* {{{ static int http_curl_progress_callback(void *, double, double, double, double) */
static int http_curl_progress_callback(void *ctx, double dltotal, double dlnow, double ultotal, double ulnow)
{
	zval *param, retval;
	http_request *request = (http_request *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);

	INIT_PZVAL(&retval);
	ZVAL_NULL(&retval);

	MAKE_STD_ZVAL(param);
	array_init(param);
	add_assoc_double(param, "dltotal", dltotal);
	add_assoc_double(param, "dlnow", dlnow);
	add_assoc_double(param, "ultotal", ultotal);
	add_assoc_double(param, "ulnow", ulnow);
	
	with_error_handling(EH_NORMAL, NULL) {
		request->_in_progress_cb = 1;
		call_user_function(EG(function_table), NULL, request->_progress_callback, &retval, 1, &param TSRMLS_CC);
		request->_in_progress_cb = 0;
	} end_error_handling();
	
	zval_ptr_dtor(&param);
	zval_dtor(&retval);
	
	return 0;
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
	
	if (request->body) {
		switch (request->body->type) {
			case HTTP_REQUEST_BODY_CSTRING:
				request->body->priv = 0;
				return CURLIOE_OK;
				break;
				
			case HTTP_REQUEST_BODY_UPLOADFILE:
				if (SUCCESS == php_stream_rewind((php_stream *) request->body->data)) {
					return CURLIOE_OK;
				}
				break;
		}
	}
	
	return CURLIOE_FAILRESTART;
}
/* }}} */

/* {{{ static int http_curl_raw_callback(CURL *, curl_infotype, char *, size_t, void *) */
static int http_curl_raw_callback(CURL *ch, curl_infotype type, char *data, size_t length, void *ctx)
{
	http_request *request = (http_request *) ctx;

#define EMPTY_HEADER(d, l) (!l || (l == 1 && d[0] == '\n') || (l == 2 && d[0] == '\r' && d[1] == '\n'))
	switch (type) {
		case CURLINFO_DATA_IN:
			if (request->conv.last_type == CURLINFO_HEADER_IN) {
				phpstr_appends(&request->conv.response, HTTP_CRLF);
			}
			phpstr_append(&request->conv.response, data, length);
			break;
		case CURLINFO_HEADER_IN:
			if (!EMPTY_HEADER(data, length)) {
				phpstr_append(&request->conv.response, data, length);
			}
			break;
		case CURLINFO_DATA_OUT:
		case CURLINFO_HEADER_OUT:
			phpstr_append(&request->conv.request, data, length);
			break;
		default:
			break;
	}

#if 0
	{
		const char _sym[] = "><><><";
		if (type) {
			for (fprintf(stderr, "%c ", _sym[type-1]); length--; data++) {
				fprintf(stderr, HTTP_IS_CTYPE(print, *data)?"%c":"\\x%02X", (int) *data);
				if (*data == '\n' && length) {
					fprintf(stderr, "\n%c ", _sym[type-1]);
				}
			}
			fprintf(stderr, "\n");
		} else {
			fprintf(stderr, "# %s", data);
		}
	}
#endif
	
	if (type) {
		request->conv.last_type = type;
	}
	return 0;
}
/* }}} */

/* {{{ static inline zval *http_request_option(http_request *, HashTable *, char *, size_t, int) */
static inline zval *_http_request_option_ex(http_request *r, HashTable *options, char *key, size_t keylen, int type TSRMLS_DC)
{
	if (options) {
		zval **zoption;
		ulong h = zend_hash_func(key, keylen);
		
		if (SUCCESS == zend_hash_quick_find(options, key, keylen, h, (void *) &zoption)) {
			zval *option, *cached;
			
			option = http_zsep(type, *zoption);
			cached = http_request_option_cache_ex(r, key, keylen, h, option);
			
			zval_ptr_dtor(&option);
			return cached;
		}
	}
	
	return NULL;
}
/* }}} */

/* {{{ static inline zval *http_request_option_cache(http_request *, char *key, zval *) */
static inline zval *_http_request_option_cache_ex(http_request *r, char *key, size_t keylen, ulong h, zval *opt TSRMLS_DC)
{
	ZVAL_ADDREF(opt);
	
	if (h) {
		zend_hash_quick_update(&r->_cache.options, key, keylen, h, &opt, sizeof(zval *), NULL);
	} else {
		zend_hash_update(&r->_cache.options, key, keylen, &opt, sizeof(zval *), NULL);
	}
	
	return opt;
}
/* }}} */

/* {{{ static inline int http_request_cookies_enabled(http_request *) */
static inline int _http_request_cookies_enabled(http_request *request) {
	http_request_storage *st;
	
	if (request->ch && (st = http_request_storage_get(request->ch)) && st->cookiestore) {
		/* cookies are enabled */
		return 1;
	}
	return 0;
}
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

