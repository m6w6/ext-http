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

/* $Id: php_http_request_api.c 298591 2010-04-26 11:46:55Z mike $ */

#include "php_http.h"

#include <Zend/zend_interfaces.h>

#if defined(ZTS) && defined(PHP_HTTP_HAVE_SSL)
#	ifdef PHP_WIN32
#		define PHP_HTTP_NEED_OPENSSL_TSL
#		include <openssl/crypto.h>
#	else /* !PHP_WIN32 */
#		if defined(PHP_HTTP_HAVE_OPENSSL)
#			define PHP_HTTP_NEED_OPENSSL_TSL
#			include <openssl/crypto.h>
#		elif defined(PHP_HTTP_HAVE_GNUTLS)
#			define PHP_HTTP_NEED_GNUTLS_TSL
#			include <gcrypt.h>
#		else
#			warning \
				"libcurl was compiled with SSL support, but configure could not determine which" \
				"library was used; thus no SSL crypto locking callbacks will be set, which may " \
				"cause random crashes on SSL requests"
#		endif /* PHP_HTTP_HAVE_OPENSSL || PHP_HTTP_HAVE_GNUTLS */
#	endif /* PHP_WIN32 */
#endif /* ZTS && PHP_HTTP_HAVE_SSL */


#ifdef PHP_HTTP_NEED_OPENSSL_TSL
static MUTEX_T *php_http_openssl_tsl = NULL;

static void php_http_openssl_thread_lock(int mode, int n, const char * file, int line)
{
	if (mode & CRYPTO_LOCK) {
		tsrm_mutex_lock(php_http_openssl_tsl[n]);
	} else {
		tsrm_mutex_unlock(php_http_openssl_tsl[n]);
	}
}

static ulong php_http_openssl_thread_id(void)
{
	return (ulong) tsrm_thread_id();
}
#endif
#ifdef PHP_HTTP_NEED_GNUTLS_TSL
static int php_http_gnutls_mutex_create(void **m)
{
	if (*((MUTEX_T *) m) = tsrm_mutex_alloc()) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

static int php_http_gnutls_mutex_destroy(void **m)
{
	tsrm_mutex_free(*((MUTEX_T *) m));
	return SUCCESS;
}

static int php_http_gnutls_mutex_lock(void **m)
{
	return tsrm_mutex_lock(*((MUTEX_T *) m));
}

static int php_http_gnutls_mutex_unlock(void **m)
{
	return tsrm_mutex_unlock(*((MUTEX_T *) m));
}

static struct gcry_thread_cbs php_http_gnutls_tsl = {
	GCRY_THREAD_OPTION_USER,
	NULL,
	php_http_gnutls_mutex_create,
	php_http_gnutls_mutex_destroy,
	php_http_gnutls_mutex_lock,
	php_http_gnutls_mutex_unlock
};
#endif


/* safe curl wrappers */
#define init_curl_storage(ch) \
	{\
		php_http_request_storage_t *st = pecalloc(1, sizeof(php_http_request_storage_t), 1); \
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
	php_http_request_storage_t *st = php_http_request_storage_get(p);
	
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

static inline zval *php_http_request_option(php_http_request_t *request, HashTable *options, char *key, size_t keylen, int type);
static inline zval *php_http_request_option_cache(php_http_request_t *r, char *key, size_t keylen, ulong h, zval *opt);
static inline int php_http_request_cookies_enabled(php_http_request_t *r);

static size_t php_http_curl_read_callback(void *, size_t, size_t, void *);
static int php_http_curl_progress_callback(void *, double, double, double, double);
static int php_http_curl_raw_callback(CURL *, curl_infotype, char *, size_t, void *);
static int php_http_curl_dummy_callback(char *data, size_t n, size_t l, void *s) { return n*l; }
static curlioerr php_http_curl_ioctl_callback(CURL *, curliocmd, void *);

PHP_HTTP_API CURL * php_http_curl_init(CURL *ch, php_http_request_t *request TSRMLS_DC)
{
	if (ch || (SUCCESS == php_http_persistent_handle_acquire(ZEND_STRL("http_request"), &ch TSRMLS_CC))) {
#if defined(ZTS)
		curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1L);
#endif
		curl_easy_setopt(ch, CURLOPT_HEADER, 0L);
		curl_easy_setopt(ch, CURLOPT_FILETIME, 1L);
		curl_easy_setopt(ch, CURLOPT_AUTOREFERER, 1L);
		curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(ch, CURLOPT_HEADERFUNCTION, NULL);
		curl_easy_setopt(ch, CURLOPT_DEBUGFUNCTION, php_http_curl_raw_callback);
		curl_easy_setopt(ch, CURLOPT_READFUNCTION, php_http_curl_read_callback);
		curl_easy_setopt(ch, CURLOPT_IOCTLFUNCTION, php_http_curl_ioctl_callback);
		curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, php_http_curl_dummy_callback);
		
		/* set context */
		if (request) {
			curl_easy_setopt(ch, CURLOPT_DEBUGDATA, request);
			
			/* attach curl handle */
			request->ch = ch;
			/* set defaults (also in php_http_request_reset()) */
			php_http_request_defaults(request);
		}
	}
	
	return ch;
}
PHP_HTTP_API CURL *php_http_curl_copy(CURL *ch TSRMLS_DC)
{
	CURL *copy;
	
	if (SUCCESS == php_http_persistent_handle_accrete(ZEND_STRL("http_request"), ch, &copy TSRMLS_CC)) {
		return copy;
	}
	return NULL;
}
PHP_HTTP_API void php_http_curl_free(CURL **ch TSRMLS_DC)
{
	if (*ch) {
		curl_easy_setopt(*ch, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(*ch, CURLOPT_PROGRESSFUNCTION, NULL);
		curl_easy_setopt(*ch, CURLOPT_VERBOSE, 0L);
		curl_easy_setopt(*ch, CURLOPT_DEBUGFUNCTION, NULL);
		
		php_http_persistent_handle_release(ZEND_STRL("http_request"), ch TSRMLS_CC);
	}
}

PHP_HTTP_API php_http_request_t *php_http_request_init(php_http_request_t *request, CURL *ch, php_http_request_method_t meth, const char *url TSRMLS_DC)
{
	php_http_request_t *r;
	
	if (request) {
		r = request;
	} else {
		r = emalloc(sizeof(php_http_request_t));
	}
	memset(r, 0, sizeof(php_http_request_t));
	
	r->ch = ch;
	r->url = (url) ? php_http_url_absolute(url, 0) : NULL;
	r->meth = (meth > 0) ? meth : PHP_HTTP_GET;

	r->parser.ctx = php_http_message_parser_init(NULL TSRMLS_CC);
	r->parser.msg = php_http_message_init(NULL, 0 TSRMLS_CC);
	r->parser.buf = php_http_buffer_init(NULL);

	php_http_buffer_init(&r->_cache.cookies);
	zend_hash_init(&r->_cache.options, 0, NULL, ZVAL_PTR_DTOR, 0);
	
	TSRMLS_SET_CTX(r->ts);
	
	return r;
}

PHP_HTTP_API void php_http_request_dtor(php_http_request_t *request)
{
	TSRMLS_FETCH_FROM_CTX(request->ts);
	
	php_http_request_reset(request);
	php_http_curl_free(&request->ch);
	php_http_message_body_free(&request->body);
	php_http_message_parser_free(&request->parser.ctx);
	php_http_message_free(&request->parser.msg);
	php_http_buffer_free(&request->parser.buf);

	php_http_buffer_dtor(&request->_cache.cookies);
	zend_hash_destroy(&request->_cache.options);
	if (request->_cache.headers) {
		curl_slist_free_all(request->_cache.headers);
		request->_cache.headers = NULL;
	}
	if (request->_progress.callback) {
		zval_ptr_dtor(&request->_progress.callback);
		request->_progress.callback = NULL;
	}
}

PHP_HTTP_API void php_http_request_free(php_http_request_t **request)
{
	if (*request) {
		TSRMLS_FETCH_FROM_CTX((*request)->ts);
		php_http_request_dtor(*request);
		efree(*request);
		*request = NULL;
	}
}

PHP_HTTP_API void php_http_request_reset(php_http_request_t *request)
{
	TSRMLS_FETCH_FROM_CTX(request->ts);
	STR_SET(request->url, NULL);
	php_http_message_body_dtor(request->body);
	php_http_request_defaults(request);
	
	if (request->ch) {
		php_http_request_storage_t *st = php_http_request_storage_get(request->ch);
		
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

PHP_HTTP_API STATUS php_http_request_enable_cookies(php_http_request_t *request)
{
	int initialized = 1;
	TSRMLS_FETCH_FROM_CTX(request->ts);
	
	PHP_HTTP_CHECK_CURL_INIT(request->ch, php_http_curl_init(request->ch, request TSRMLS_CC), initialized = 0);
	if (initialized && (php_http_request_cookies_enabled(request) || (CURLE_OK == curl_easy_setopt(request->ch, CURLOPT_COOKIEFILE, "")))) {
		return SUCCESS;
	}
	php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Could not enable cookies for this session");
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_request_reset_cookies(php_http_request_t *request, int session_only)
{
	int initialized = 1;
	TSRMLS_FETCH_FROM_CTX(request->ts);
	
	PHP_HTTP_CHECK_CURL_INIT(request->ch, php_http_curl_init(request->ch, request TSRMLS_CC), initialized = 0);
	if (initialized) {
		if (!php_http_request_cookies_enabled(request)) {
			if (SUCCESS != php_http_request_enable_cookies(request)) {
				return FAILURE;
			}
		}
		if (session_only) {
			if (CURLE_OK == curl_easy_setopt(request->ch, CURLOPT_COOKIELIST, "SESS")) {
				return SUCCESS;
			}
		} else {
			if (CURLE_OK == curl_easy_setopt(request->ch, CURLOPT_COOKIELIST, "ALL")) {
				return SUCCESS;
			}
		}
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_request_flush_cookies(php_http_request_t *request)
{
	int initialized = 1;
	TSRMLS_FETCH_FROM_CTX(request->ts);
	
	PHP_HTTP_CHECK_CURL_INIT(request->ch, php_http_curl_init(request->ch, request TSRMLS_CC), initialized = 0);
	if (initialized) {
		if (!php_http_request_cookies_enabled(request)) {
			return FAILURE;
		}
		if (CURLE_OK == curl_easy_setopt(request->ch, CURLOPT_COOKIELIST, "FLUSH")) {
			return SUCCESS;
		}
	}
	return FAILURE;
}

PHP_HTTP_API void php_http_request_defaults(php_http_request_t *request)
{
	if (request->ch) {
		PHP_HTTP_CURL_OPT(CURLOPT_NOPROGRESS, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_PROGRESSDATA, request);
		PHP_HTTP_CURL_OPT(CURLOPT_PROGRESSFUNCTION, php_http_curl_progress_callback);
		PHP_HTTP_CURL_OPT(CURLOPT_URL, NULL);
#if PHP_HTTP_CURL_VERSION(7,19,4)
		PHP_HTTP_CURL_OPT(CURLOPT_NOPROXY, NULL);
#endif
		PHP_HTTP_CURL_OPT(CURLOPT_PROXY, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_PROXYPORT, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_PROXYTYPE, 0L);
		/* libcurl < 7.19.6 does not clear auth info with USERPWD set to NULL */
#if PHP_HTTP_CURL_VERSION(7,19,1)		
		PHP_HTTP_CURL_OPT(CURLOPT_PROXYUSERNAME, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_PROXYPASSWORD, NULL);
#endif
		PHP_HTTP_CURL_OPT(CURLOPT_PROXYAUTH, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_HTTPPROXYTUNNEL, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_DNS_CACHE_TIMEOUT, 60L);
		PHP_HTTP_CURL_OPT(CURLOPT_IPRESOLVE, 0);
		PHP_HTTP_CURL_OPT(CURLOPT_LOW_SPEED_LIMIT, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_LOW_SPEED_TIME, 0L);
		/* LFS weirdance
		PHP_HTTP_CURL_OPT(CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t) 0);
		PHP_HTTP_CURL_OPT(CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t) 0);
		*/
		/* crashes
		PHP_HTTP_CURL_OPT(CURLOPT_MAXCONNECTS, 5L); */
		PHP_HTTP_CURL_OPT(CURLOPT_FRESH_CONNECT, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_FORBID_REUSE, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_INTERFACE, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_PORT, 0L);
#if PHP_HTTP_CURL_VERSION(7,19,0)
		PHP_HTTP_CURL_OPT(CURLOPT_ADDRESS_SCOPE, 0L);
#endif
		PHP_HTTP_CURL_OPT(CURLOPT_LOCALPORT, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_LOCALPORTRANGE, 0L);
		/* libcurl < 7.19.6 does not clear auth info with USERPWD set to NULL */
#if PHP_HTTP_CURL_VERSION(7,19,1)
		PHP_HTTP_CURL_OPT(CURLOPT_USERNAME, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_PASSWORD, NULL);
#endif
		PHP_HTTP_CURL_OPT(CURLOPT_HTTPAUTH, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_ENCODING, NULL);
		/* we do this ourself anyway */
		PHP_HTTP_CURL_OPT(CURLOPT_HTTP_CONTENT_DECODING, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_HTTP_TRANSFER_DECODING, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_FOLLOWLOCATION, 0L);
#if PHP_HTTP_CURL_VERSION(7,19,1)
		PHP_HTTP_CURL_OPT(CURLOPT_POSTREDIR, 0L);
#else
		PHP_HTTP_CURL_OPT(CURLOPT_POST301, 0L);
#endif
		PHP_HTTP_CURL_OPT(CURLOPT_UNRESTRICTED_AUTH, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_REFERER, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_USERAGENT, "PECL::HTTP/" PHP_HTTP_EXT_VERSION " (PHP/" PHP_VERSION ")");
		PHP_HTTP_CURL_OPT(CURLOPT_HTTPHEADER, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_COOKIE, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_COOKIESESSION, 0L);
		/* these options would enable curl's cookie engine by default which we don't want
		PHP_HTTP_CURL_OPT(CURLOPT_COOKIEFILE, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_COOKIEJAR, NULL); */
		PHP_HTTP_CURL_OPT(CURLOPT_COOKIELIST, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_RANGE, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_RESUME_FROM, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_MAXFILESIZE, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_TIMECONDITION, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_TIMEVALUE, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_TIMEOUT, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_CONNECTTIMEOUT, 3);
		PHP_HTTP_CURL_OPT(CURLOPT_SSLCERT, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_SSLCERTTYPE, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_SSLCERTPASSWD, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_SSLKEY, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_SSLKEYTYPE, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_SSLKEYPASSWD, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_SSLENGINE, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_SSLVERSION, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_SSL_VERIFYPEER, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_SSL_VERIFYHOST, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_SSL_CIPHER_LIST, NULL);
#if PHP_HTTP_CURL_VERSION(7,19,0)
		PHP_HTTP_CURL_OPT(CURLOPT_ISSUERCERT, NULL);
	#if defined(PHP_HTTP_HAVE_OPENSSL)
		PHP_HTTP_CURL_OPT(CURLOPT_CRLFILE, NULL);
	#endif
#endif
#if PHP_HTTP_CURL_VERSION(7,19,1) && defined(PHP_HTTP_HAVE_OPENSSL)
		PHP_HTTP_CURL_OPT(CURLOPT_CERTINFO, NULL);
#endif
#ifdef PHP_HTTP_CURL_CAINFO
		PHP_HTTP_CURL_OPT(CURLOPT_CAINFO, PHP_HTTP_CURL_CAINFO);
#else
		PHP_HTTP_CURL_OPT(CURLOPT_CAINFO, NULL);
#endif
		PHP_HTTP_CURL_OPT(CURLOPT_CAPATH, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_RANDOM_FILE, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_EGDSOCKET, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_POSTFIELDS, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_POSTFIELDSIZE, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_HTTPPOST, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_IOCTLDATA, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_READDATA, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_INFILESIZE, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);
		PHP_HTTP_CURL_OPT(CURLOPT_CUSTOMREQUEST, NULL);
		PHP_HTTP_CURL_OPT(CURLOPT_NOBODY, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_POST, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_UPLOAD, 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_HTTPGET, 1L);
	}
}

PHP_HTTP_API void php_http_request_set_progress_callback(php_http_request_t *request, zval *cb)
{
	if (request->_progress.callback) {
		zval_ptr_dtor(&request->_progress.callback);
	}
	if ((request->_progress.callback = cb)) {
		Z_ADDREF_P(cb);
	}
}

PHP_HTTP_API STATUS php_http_request_prepare(php_http_request_t *request, HashTable *options)
{
	zval *zoption;
	zend_bool range_req = 0;
	php_http_request_storage_t *storage;

	TSRMLS_FETCH_FROM_CTX(request->ts);
	
	PHP_HTTP_CHECK_CURL_INIT(request->ch, php_http_curl_init(NULL, request TSRMLS_CC), return FAILURE);
	
	if (!(storage = php_http_request_storage_get(request->ch))) {
		return FAILURE;
	}
	storage->errorbuffer[0] = '\0';
	/* set options */
	if (storage->url) {
		pefree(storage->url, 1);
	}
	storage->url = pestrdup(request->url, 1);
	PHP_HTTP_CURL_OPT(CURLOPT_URL, storage->url);

	/* progress callback */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("onprogress"), -1))) {
		php_http_request_set_progress_callback(request, zoption);
	}

	/* proxy */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("proxyhost"), IS_STRING))) {
		PHP_HTTP_CURL_OPT(CURLOPT_PROXY, Z_STRVAL_P(zoption));
		/* type */
		if ((zoption = php_http_request_option(request, options, ZEND_STRS("proxytype"), IS_LONG))) {
			PHP_HTTP_CURL_OPT(CURLOPT_PROXYTYPE, Z_LVAL_P(zoption));
		}
		/* port */
		if ((zoption = php_http_request_option(request, options, ZEND_STRS("proxyport"), IS_LONG))) {
			PHP_HTTP_CURL_OPT(CURLOPT_PROXYPORT, Z_LVAL_P(zoption));
		}
		/* user:pass */
		if ((zoption = php_http_request_option(request, options, ZEND_STRS("proxyauth"), IS_STRING)) && Z_STRLEN_P(zoption)) {
			PHP_HTTP_CURL_OPT(CURLOPT_PROXYUSERPWD, Z_STRVAL_P(zoption));
		}
		/* auth method */
		if ((zoption = php_http_request_option(request, options, ZEND_STRS("proxyauthtype"), IS_LONG))) {
			PHP_HTTP_CURL_OPT(CURLOPT_PROXYAUTH, Z_LVAL_P(zoption));
		}
		/* tunnel */
		if ((zoption = php_http_request_option(request, options, ZEND_STRS("proxytunnel"), IS_BOOL)) && Z_BVAL_P(zoption)) {
			PHP_HTTP_CURL_OPT(CURLOPT_HTTPPROXYTUNNEL, 1L);
		}
	}
#if PHP_HTTP_CURL_VERSION(7,19,4)
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("noproxy"), IS_STRING))) {
		PHP_HTTP_CURL_OPT(CURLOPT_NOPROXY, Z_STRVAL_P(zoption));
	}
#endif

	/* dns */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("dns_cache_timeout"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_DNS_CACHE_TIMEOUT, Z_LVAL_P(zoption));
	}
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("ipresolve"), IS_LONG)) && Z_LVAL_P(zoption)) {
		PHP_HTTP_CURL_OPT(CURLOPT_IPRESOLVE, Z_LVAL_P(zoption));
	}
	
	/* limits */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("low_speed_limit"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_LOW_SPEED_LIMIT, Z_LVAL_P(zoption));
	}
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("low_speed_time"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_LOW_SPEED_TIME, Z_LVAL_P(zoption));
	}
	/* LSF weirdance
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("max_send_speed"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t) Z_LVAL_P(zoption));
	}
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("max_recv_speed"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t) Z_LVAL_P(zoption));
	}
	*/
	/* crashes
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("maxconnects"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_MAXCONNECTS, Z_LVAL_P(zoption));
	} */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("fresh_connect"), IS_BOOL)) && Z_BVAL_P(zoption)) {
		PHP_HTTP_CURL_OPT(CURLOPT_FRESH_CONNECT, 1L);
	}
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("forbid_reuse"), IS_BOOL)) && Z_BVAL_P(zoption)) {
		PHP_HTTP_CURL_OPT(CURLOPT_FORBID_REUSE, 1L);
	}
	
	/* outgoing interface */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("interface"), IS_STRING))) {
		PHP_HTTP_CURL_OPT(CURLOPT_INTERFACE, Z_STRVAL_P(zoption));
		
		if ((zoption = php_http_request_option(request, options, ZEND_STRS("portrange"), IS_ARRAY))) {
			zval **prs, **pre;
			
			zend_hash_internal_pointer_reset(Z_ARRVAL_P(zoption));
			if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void *) &prs)) {
				zend_hash_move_forward(Z_ARRVAL_P(zoption));
				if (SUCCESS == zend_hash_get_current_data(Z_ARRVAL_P(zoption), (void *) &pre)) {
					zval *prs_cpy = php_http_zsep(IS_LONG, *prs);
					zval *pre_cpy = php_http_zsep(IS_LONG, *pre);
					
					if (Z_LVAL_P(prs_cpy) && Z_LVAL_P(pre_cpy)) {
						PHP_HTTP_CURL_OPT(CURLOPT_LOCALPORT, MIN(Z_LVAL_P(prs_cpy), Z_LVAL_P(pre_cpy)));
						PHP_HTTP_CURL_OPT(CURLOPT_LOCALPORTRANGE, labs(Z_LVAL_P(prs_cpy)-Z_LVAL_P(pre_cpy))+1L);
					}
					zval_ptr_dtor(&prs_cpy);
					zval_ptr_dtor(&pre_cpy);
				}
			}
		}
	}

	/* another port */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("port"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_PORT, Z_LVAL_P(zoption));
	}
	
	/* RFC4007 zone_id */
#if PHP_HTTP_CURL_VERSION(7,19,0)
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("address_scope"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_ADDRESS_SCOPE, Z_LVAL_P(zoption));
	}
#endif

	/* auth */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("httpauth"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		PHP_HTTP_CURL_OPT(CURLOPT_USERPWD, Z_STRVAL_P(zoption));
	}
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("httpauthtype"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_HTTPAUTH, Z_LVAL_P(zoption));
	}

	/* redirects, defaults to 0 */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("redirect"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_FOLLOWLOCATION, Z_LVAL_P(zoption) ? 1L : 0L);
		PHP_HTTP_CURL_OPT(CURLOPT_MAXREDIRS, request->_cache.redirects = Z_LVAL_P(zoption));
		if ((zoption = php_http_request_option(request, options, ZEND_STRS("unrestrictedauth"), IS_BOOL))) {
			PHP_HTTP_CURL_OPT(CURLOPT_UNRESTRICTED_AUTH, Z_LVAL_P(zoption));
		}
		if ((zoption = php_http_request_option(request, options, ZEND_STRS("postredir"), IS_BOOL))) {
#if PHP_HTTP_CURL_VERSION(7,19,1)
			PHP_HTTP_CURL_OPT(CURLOPT_POSTREDIR, Z_BVAL_P(zoption) ? 1L : 0L);
#else
			PHP_HTTP_CURL_OPT(CURLOPT_POST301, Z_BVAL_P(zoption) ? 1L : 0L);
#endif
		}
	} else {
		request->_cache.redirects = 0;
	}
	
	/* retries, defaults to 0 */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("retrycount"), IS_LONG))) {
		request->_retry.count = Z_LVAL_P(zoption);
		if ((zoption = php_http_request_option(request, options, ZEND_STRS("retrydelay"), IS_DOUBLE))) {
			request->_retry.delay = Z_DVAL_P(zoption);
		} else {
			request->_retry.delay = 0;
		}
	} else {
		request->_retry.count = 0;
	}

	/* referer */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("referer"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		PHP_HTTP_CURL_OPT(CURLOPT_REFERER, Z_STRVAL_P(zoption));
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("useragent"), IS_STRING))) {
		/* allow to send no user agent, not even default one */
		if (Z_STRLEN_P(zoption)) {
			PHP_HTTP_CURL_OPT(CURLOPT_USERAGENT, Z_STRVAL_P(zoption));
		} else {
			PHP_HTTP_CURL_OPT(CURLOPT_USERAGENT, NULL);
		}
	}

	/* resume */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("resume"), IS_LONG)) && (Z_LVAL_P(zoption) > 0)) {
		range_req = 1;
		PHP_HTTP_CURL_OPT(CURLOPT_RESUME_FROM, Z_LVAL_P(zoption));
	}
	/* or range of kind array(array(0,499), array(100,1499)) */
	else if ((zoption = php_http_request_option(request, options, ZEND_STRS("range"), IS_ARRAY)) && zend_hash_num_elements(Z_ARRVAL_P(zoption))) {
		HashPosition pos1, pos2;
		zval **rr, **rb, **re;
		php_http_buffer_t rs;
		
		php_http_buffer_init(&rs);
		FOREACH_VAL(pos1, zoption, rr) {
			if (Z_TYPE_PP(rr) == IS_ARRAY) {
				zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(rr), &pos2);
				if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(rr), (void *) &rb, &pos2)) {
					zend_hash_move_forward_ex(Z_ARRVAL_PP(rr), &pos2);
					if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_PP(rr), (void *) &re, &pos2)) {
						if (	((Z_TYPE_PP(rb) == IS_LONG) || ((Z_TYPE_PP(rb) == IS_STRING) && is_numeric_string(Z_STRVAL_PP(rb), Z_STRLEN_PP(rb), NULL, NULL, 1))) &&
								((Z_TYPE_PP(re) == IS_LONG) || ((Z_TYPE_PP(re) == IS_STRING) && is_numeric_string(Z_STRVAL_PP(re), Z_STRLEN_PP(re), NULL, NULL, 1)))) {
							zval *rbl = php_http_zsep(IS_LONG, *rb);
							zval *rel = php_http_zsep(IS_LONG, *re);
							
							if ((Z_LVAL_P(rbl) >= 0) && (Z_LVAL_P(rel) >= 0)) {
								php_http_buffer_appendf(&rs, "%ld-%ld,", Z_LVAL_P(rbl), Z_LVAL_P(rel));
							}
							zval_ptr_dtor(&rbl);
							zval_ptr_dtor(&rel);
						}
					}
				}
			}
		}
		
		if (PHP_HTTP_BUFFER_LEN(&rs)) {
			zval *cached_range;
			
			/* ditch last comma */
			PHP_HTTP_BUFFER_VAL(&rs)[PHP_HTTP_BUFFER_LEN(&rs)-- -1] = '\0';
			/* cache string */
			MAKE_STD_ZVAL(cached_range);
			ZVAL_STRINGL(cached_range, PHP_HTTP_BUFFER_VAL(&rs), PHP_HTTP_BUFFER_LEN(&rs), 0);
			PHP_HTTP_CURL_OPT(CURLOPT_RANGE, Z_STRVAL_P(php_http_request_option_cache(request, ZEND_STRS("range"), 0, cached_range)));
			zval_ptr_dtor(&cached_range);
		}
	}

	/* additional headers, array('name' => 'value') */
	if (request->_cache.headers) {
		curl_slist_free_all(request->_cache.headers);
		request->_cache.headers = NULL;
	}
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("headers"), IS_ARRAY))) {
		php_http_array_hashkey_t header_key = php_http_array_hashkey_init(0);
		zval **header_val;
		HashPosition pos;
		php_http_buffer_t header;
		
		php_http_buffer_init(&header);
		FOREACH_KEYVAL(pos, zoption, header_key, header_val) {
			if (header_key.type == HASH_KEY_IS_STRING) {
				zval *header_cpy = php_http_zsep(IS_STRING, *header_val);
				
				if (!strcasecmp(header_key.str, "range")) {
					range_req = 1;
				}

				php_http_buffer_appendf(&header, "%s: %s", header_key.str, Z_STRVAL_P(header_cpy));
				php_http_buffer_fix(&header);
				request->_cache.headers = curl_slist_append(request->_cache.headers, PHP_HTTP_BUFFER_VAL(&header));
				php_http_buffer_reset(&header);
				
				zval_ptr_dtor(&header_cpy);
			}
		}
		php_http_buffer_dtor(&header);
	}
	/* etag */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("etag"), IS_STRING)) && Z_STRLEN_P(zoption)) {
		zend_bool is_quoted = !((Z_STRVAL_P(zoption)[0] != '"') || (Z_STRVAL_P(zoption)[Z_STRLEN_P(zoption)-1] != '"'));
		php_http_buffer_t header;
		
		php_http_buffer_init(&header);
		php_http_buffer_appendf(&header, is_quoted?"%s: %s":"%s: \"%s\"", range_req?"If-Match":"If-None-Match", Z_STRVAL_P(zoption));
		php_http_buffer_fix(&header);
		request->_cache.headers = curl_slist_append(request->_cache.headers, PHP_HTTP_BUFFER_VAL(&header));
		php_http_buffer_dtor(&header);
	}
	/* compression */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("compress"), IS_BOOL)) && Z_LVAL_P(zoption)) {
		request->_cache.headers = curl_slist_append(request->_cache.headers, "Accept-Encoding: gzip;q=1.0,deflate;q=0.5");
	}
	PHP_HTTP_CURL_OPT(CURLOPT_HTTPHEADER, request->_cache.headers);

	/* lastmodified */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("lastmodified"), IS_LONG))) {
		if (Z_LVAL_P(zoption)) {
			if (Z_LVAL_P(zoption) > 0) {
				PHP_HTTP_CURL_OPT(CURLOPT_TIMEVALUE, Z_LVAL_P(zoption));
			} else {
				PHP_HTTP_CURL_OPT(CURLOPT_TIMEVALUE, (long) PHP_HTTP_G->env.request.time + Z_LVAL_P(zoption));
			}
			PHP_HTTP_CURL_OPT(CURLOPT_TIMECONDITION, (long) (range_req ? CURL_TIMECOND_IFUNMODSINCE : CURL_TIMECOND_IFMODSINCE));
		} else {
			PHP_HTTP_CURL_OPT(CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
		}
	}

	/* cookies, array('name' => 'value') */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("cookies"), IS_ARRAY))) {
		php_http_buffer_dtor(&request->_cache.cookies);
		if (zend_hash_num_elements(Z_ARRVAL_P(zoption))) {
			zval *urlenc_cookies = NULL;
			/* check whether cookies should not be urlencoded; default is to urlencode them */
			if ((!(urlenc_cookies = php_http_request_option(request, options, ZEND_STRS("encodecookies"), IS_BOOL))) || Z_BVAL_P(urlenc_cookies)) {
				if (SUCCESS == php_http_url_encode_hash_recursive(HASH_OF(zoption), &request->_cache.cookies, "; ", lenof("; "), NULL, 0 TSRMLS_CC)) {
					php_http_buffer_fix(&request->_cache.cookies);
					PHP_HTTP_CURL_OPT(CURLOPT_COOKIE, request->_cache.cookies.data);
				}
			} else {
				HashPosition pos;
				php_http_array_hashkey_t cookie_key = php_http_array_hashkey_init(0);
				zval **cookie_val;
				
				FOREACH_KEYVAL(pos, zoption, cookie_key, cookie_val) {
					if (cookie_key.type == HASH_KEY_IS_STRING) {
						zval *val = php_http_zsep(IS_STRING, *cookie_val);
						php_http_buffer_appendf(&request->_cache.cookies, "%s=%s; ", cookie_key.str, Z_STRVAL_P(val));
						zval_ptr_dtor(&val);
					}
				}
				
				php_http_buffer_fix(&request->_cache.cookies);
				if (PHP_HTTP_BUFFER_LEN(&request->_cache.cookies)) {
					PHP_HTTP_CURL_OPT(CURLOPT_COOKIE, PHP_HTTP_BUFFER_VAL(&request->_cache.cookies));
				}
			}
		}
	}

	/* don't load session cookies from cookiestore */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("cookiesession"), IS_BOOL)) && Z_BVAL_P(zoption)) {
		PHP_HTTP_CURL_OPT(CURLOPT_COOKIESESSION, 1L);
	}

	/* cookiestore, read initial cookies from that file and store cookies back into that file */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("cookiestore"), IS_STRING))) {
		if (Z_STRLEN_P(zoption)) {
			if (SUCCESS != php_check_open_basedir(Z_STRVAL_P(zoption) TSRMLS_CC)) {
				return FAILURE;
			}
		}
		if (storage->cookiestore) {
			pefree(storage->cookiestore, 1);
		}
		storage->cookiestore = pestrndup(Z_STRVAL_P(zoption), Z_STRLEN_P(zoption), 1);
		PHP_HTTP_CURL_OPT(CURLOPT_COOKIEFILE, storage->cookiestore);
		PHP_HTTP_CURL_OPT(CURLOPT_COOKIEJAR, storage->cookiestore);
	}

	/* maxfilesize */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("maxfilesize"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_MAXFILESIZE, Z_LVAL_P(zoption));
	}

	/* http protocol */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("protocol"), IS_LONG))) {
		PHP_HTTP_CURL_OPT(CURLOPT_HTTP_VERSION, Z_LVAL_P(zoption));
	}

	/* timeout, defaults to 0 */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("timeout"), IS_DOUBLE))) {
		PHP_HTTP_CURL_OPT(CURLOPT_TIMEOUT_MS, (long)(Z_DVAL_P(zoption)*1000));
	}
	/* connecttimeout, defaults to 0 */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("connecttimeout"), IS_DOUBLE))) {
		PHP_HTTP_CURL_OPT(CURLOPT_CONNECTTIMEOUT_MS, (long)(Z_DVAL_P(zoption)*1000));
	}

	/* ssl */
	if ((zoption = php_http_request_option(request, options, ZEND_STRS("ssl"), IS_ARRAY))) {
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
		zval **param;
		HashPosition pos;

		FOREACH_KEYVAL(pos, zoption, key, param) {
			if (key.type == HASH_KEY_IS_STRING) {
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLCERT, 0, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLCERTTYPE, 0, 0);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLCERTPASSWD, 0, 0);

				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLKEY, 0, 0);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLKEYTYPE, 0, 0);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLKEYPASSWD, 0, 0);

				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSLENGINE, 0, 0);
				PHP_HTTP_CURL_OPT_LONG(CURLOPT_SSLVERSION, 0);

				PHP_HTTP_CURL_OPT_LONG(CURLOPT_SSL_VERIFYPEER, 1);
				PHP_HTTP_CURL_OPT_LONG(CURLOPT_SSL_VERIFYHOST, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_SSL_CIPHER_LIST, 1, 0);

				PHP_HTTP_CURL_OPT_STRING(CURLOPT_CAINFO, -3, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_CAPATH, -3, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_RANDOM_FILE, -3, 1);
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_EGDSOCKET, -3, 1);
#if PHP_HTTP_CURL_VERSION(7,19,0)
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_ISSUERCERT, -3, 1);
	#if defined(PHP_HTTP_HAVE_OPENSSL)
				PHP_HTTP_CURL_OPT_STRING(CURLOPT_CRLFILE, -3, 1);
	#endif
#endif
#if PHP_HTTP_CURL_VERSION(7,19,1) && defined(PHP_HTTP_HAVE_OPENSSL)
				PHP_HTTP_CURL_OPT_LONG(CURLOPT_CERTINFO, -3);
#endif
			}
		}
	}

	/* request method */
	switch (request->meth) {
		case PHP_HTTP_GET:
			PHP_HTTP_CURL_OPT(CURLOPT_HTTPGET, 1L);
			break;

		case PHP_HTTP_HEAD:
			PHP_HTTP_CURL_OPT(CURLOPT_NOBODY, 1L);
			break;

		case PHP_HTTP_POST:
			PHP_HTTP_CURL_OPT(CURLOPT_POST, 1L);
			break;

		case PHP_HTTP_PUT:
			PHP_HTTP_CURL_OPT(CURLOPT_UPLOAD, 1L);
			break;

		default: {
			const char *meth = php_http_request_method_name(request->meth);

			if (meth) {
				PHP_HTTP_CURL_OPT(CURLOPT_CUSTOMREQUEST, meth);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_METHOD, "Unsupported request method: %d (%s)", request->meth, request->url);
				return FAILURE;
			}
			break;
		}
	}

	/* attach request body */
	if (request->body && (request->meth != PHP_HTTP_GET) && (request->meth != PHP_HTTP_HEAD) && (request->meth != PHP_HTTP_OPTIONS)) {
		if (1 || request->meth == PHP_HTTP_PUT) {
			/* PUT/UPLOAD _needs_ READDATA */
			PHP_HTTP_CURL_OPT(CURLOPT_IOCTLDATA, request);
			PHP_HTTP_CURL_OPT(CURLOPT_READDATA, request);
			PHP_HTTP_CURL_OPT(CURLOPT_INFILESIZE, php_http_message_body_size(request->body));
		} else {
			abort();
			//PHP_HTTP_CURL_OPT(CURLOPT_POSTFIELDS, request->body->real->data);
			PHP_HTTP_CURL_OPT(CURLOPT_POSTFIELDSIZE, php_http_message_body_size(request->body));
		}
	}

	return SUCCESS;
}

PHP_HTTP_API void php_http_request_exec(php_http_request_t *request)
{
	uint tries = 0;
	CURLcode result;
	TSRMLS_FETCH_FROM_CTX(request->ts);
	
retry:
	if (CURLE_OK != (result = curl_easy_perform(request->ch))) {
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(result), php_http_request_storage_get(request->ch)->errorbuffer, request->url);
		if (EG(exception)) {
			add_property_long(EG(exception), "curlCode", result);
		}
		
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
					if (request->_retry.delay >= PHP_HTTP_DIFFSEC) {
						php_http_sleep(request->_retry.delay);
					}
					goto retry;
				default:
					break;
			}
		}
	}
}

static size_t php_http_curl_read_callback(void *data, size_t len, size_t n, void *ctx)
{
	php_http_request_t *request = (php_http_request_t *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->ts);
	
	if (request->body) {
		return php_stream_read(php_http_message_body_stream(request->body), data, len * n);
	}
	return 0;
}

static int php_http_curl_progress_callback(void *ctx, double dltotal, double dlnow, double ultotal, double ulnow)
{
	php_http_request_t *request = (php_http_request_t *) ctx;

	request->_progress.state.dl.total = dltotal;
	request->_progress.state.dl.now = dlnow;
	request->_progress.state.ul.total = ultotal;
	request->_progress.state.ul.now = ulnow;

	if (request->_progress.callback) {
		zval *param, retval;
		TSRMLS_FETCH_FROM_CTX(request->ts);
	
		INIT_PZVAL(&retval);
		ZVAL_NULL(&retval);
	
		MAKE_STD_ZVAL(param);
		array_init(param);
		add_assoc_double(param, "dltotal", request->_progress.state.dl.total);
		add_assoc_double(param, "dlnow", request->_progress.state.dl.now);
		add_assoc_double(param, "ultotal", request->_progress.state.ul.total);
		add_assoc_double(param, "ulnow", request->_progress.state.ul.now);

		with_error_handling(EH_NORMAL, NULL) {
			request->_progress.in_cb = 1;
			call_user_function(EG(function_table), NULL, request->_progress.callback, &retval, 1, &param TSRMLS_CC);
			request->_progress.in_cb = 0;
		} end_error_handling();

		zval_ptr_dtor(&param);
		zval_dtor(&retval);
	}
	
	return 0;
}

static curlioerr php_http_curl_ioctl_callback(CURL *ch, curliocmd cmd, void *ctx)
{
	php_http_request_t *request = (php_http_request_t *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->ts);
	
	if (cmd != CURLIOCMD_RESTARTREAD) {
		return CURLIOE_UNKNOWNCMD;
	}
	
	if (request->body) {
		if (SUCCESS == php_stream_rewind(php_http_message_body_stream(request->body))) {
			return CURLIOE_OK;
		}
	}
	
	return CURLIOE_FAILRESTART;
}

static int php_http_curl_raw_callback(CURL *ch, curl_infotype type, char *data, size_t length, void *ctx)
{
	php_http_request_t *request = (php_http_request_t *) ctx;

	/* process data */
	switch (type) {
		case CURLINFO_HEADER_IN:
		case CURLINFO_DATA_IN:
		case CURLINFO_HEADER_OUT:
		case CURLINFO_DATA_OUT:
			php_http_buffer_append(request->parser.buf, data, length);
			if (PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE == php_http_message_parser_parse(request->parser.ctx, request->parser.buf, request->_cache.redirects?PHP_HTTP_MESSAGE_PARSER_EMPTY_REDIRECTS:0, &request->parser.msg)) {
				return -1;
			}
			break;
		default:
			break;
	}

	/* debug */
#if 0
	{
		const char _sym[] = "><><><";
		if (type) {
			for (fprintf(stderr, "%c ", _sym[type-1]); length--; data++) {
				fprintf(stderr, PHP_HTTP_IS_CTYPE(print, *data)?"%c":"\\x%02X", (int) *data);
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
	
	return 0;
}

static inline zval *php_http_request_option(php_http_request_t *r, HashTable *options, char *key, size_t keylen, int type)
{
	TSRMLS_FETCH_FROM_CTX(r->ts);

	if (options) {
		zval **zoption;
		ulong h = zend_hash_func(key, keylen);
		
		if (SUCCESS == zend_hash_quick_find(options, key, keylen, h, (void *) &zoption)) {
			zval *option, *cached;
			
			option = php_http_zsep(type, *zoption);
			cached = php_http_request_option_cache(r, key, keylen, h, option);
			
			zval_ptr_dtor(&option);
			return cached;
		}
	}
	
	return NULL;
}

static inline zval *php_http_request_option_cache(php_http_request_t *r, char *key, size_t keylen, ulong h, zval *opt)
{
	TSRMLS_FETCH_FROM_CTX(r->ts);
	Z_ADDREF_P(opt);
	
	if (h) {
		zend_hash_quick_update(&r->_cache.options, key, keylen, h, &opt, sizeof(zval *), NULL);
	} else {
		zend_hash_update(&r->_cache.options, key, keylen, &opt, sizeof(zval *), NULL);
	}
	
	return opt;
}

static inline int php_http_request_cookies_enabled(php_http_request_t *request) {
	php_http_request_storage_t *st;
	
	if (request->ch && (st = php_http_request_storage_get(request->ch)) && st->cookiestore) {
		/* cookies are enabled */
		return 1;
	}
	return 0;
}

/* USERLAND */

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpRequest, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpRequest, method, 0)
#define PHP_HTTP_REQUEST_ME(method, visibility)	PHP_ME(HttpRequest, method, PHP_HTTP_ARGS(HttpRequest, method), visibility)
#define PHP_HTTP_REQUEST_ALIAS(method, func)	PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpRequest, method))
#define PHP_HTTP_REQUEST_MALIAS(me, al, vis)	ZEND_FENTRY(me, ZEND_MN(HttpRequest_##al), PHP_HTTP_ARGS(HttpRequest, al), vis)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(method, 0)
	PHP_HTTP_ARG_VAL(options, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(factory, 0)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(method, 0)
	PHP_HTTP_ARG_VAL(options, 0)
	PHP_HTTP_ARG_VAL(class_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getOptions);
PHP_HTTP_BEGIN_ARGS(setOptions, 0)
	PHP_HTTP_ARG_VAL(options, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getSslOptions);
PHP_HTTP_BEGIN_ARGS(setSslOptions, 0)
	PHP_HTTP_ARG_VAL(ssl_options, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addSslOptions, 0)
	PHP_HTTP_ARG_VAL(ssl_optins, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getHeaders);
PHP_HTTP_BEGIN_ARGS(setHeaders, 0)
	PHP_HTTP_ARG_VAL(headers, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addHeaders, 1)
	PHP_HTTP_ARG_VAL(headers, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getCookies);
PHP_HTTP_BEGIN_ARGS(setCookies, 0)
	PHP_HTTP_ARG_VAL(cookies, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addCookies, 1)
	PHP_HTTP_ARG_VAL(cookies, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(enableCookies);
PHP_HTTP_BEGIN_ARGS(resetCookies, 0)
	PHP_HTTP_ARG_VAL(session_only, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_EMPTY_ARGS(flushCookies);

PHP_HTTP_EMPTY_ARGS(getUrl);
PHP_HTTP_BEGIN_ARGS(setUrl, 1)
	PHP_HTTP_ARG_VAL(url, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getMethod);
PHP_HTTP_BEGIN_ARGS(setMethod, 1)
	PHP_HTTP_ARG_VAL(request_method, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getContentType);
PHP_HTTP_BEGIN_ARGS(setContentType, 1)
	PHP_HTTP_ARG_VAL(content_type, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getQueryData);
PHP_HTTP_BEGIN_ARGS(setQueryData, 0)
	PHP_HTTP_ARG_VAL(query_data, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addQueryData, 1)
	PHP_HTTP_ARG_VAL(query_data, 0)
PHP_HTTP_END_ARGS;


PHP_HTTP_EMPTY_ARGS(getBody);
PHP_HTTP_BEGIN_ARGS(setBody, 0)
	PHP_HTTP_ARG_OBJ(http\\message\\Body, body, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addBody, 1)
	PHP_HTTP_ARG_OBJ(http\\message\\Body, body, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getResponseCookies, 0)
	PHP_HTTP_ARG_VAL(flags, 0)
	PHP_HTTP_ARG_VAL(allowed_extras, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseBody);
PHP_HTTP_EMPTY_ARGS(getResponseCode);
PHP_HTTP_EMPTY_ARGS(getResponseStatus);
PHP_HTTP_BEGIN_ARGS(getResponseInfo, 0)
	PHP_HTTP_ARG_VAL(name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getResponseHeader, 0)
	PHP_HTTP_ARG_VAL(header_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getMessageClass);
PHP_HTTP_BEGIN_ARGS(setMessageClass, 1)
	PHP_HTTP_ARG_VAL(message_class_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseMessage);
PHP_HTTP_EMPTY_ARGS(getRawResponseMessage);
PHP_HTTP_EMPTY_ARGS(getRequestMessage);
PHP_HTTP_EMPTY_ARGS(getRawRequestMessage);
PHP_HTTP_EMPTY_ARGS(getHistory);
PHP_HTTP_EMPTY_ARGS(clearHistory);
PHP_HTTP_EMPTY_ARGS(send);

PHP_HTTP_BEGIN_ARGS(get, 1)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(options, 0)
	PHP_HTTP_ARG_VAL(info, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(head, 1)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(options, 0)
	PHP_HTTP_ARG_VAL(info, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(postData, 2)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(data, 0)
	PHP_HTTP_ARG_VAL(options, 0)
	PHP_HTTP_ARG_VAL(info, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(postFields, 2)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(data, 0)
	PHP_HTTP_ARG_VAL(options, 0)
	PHP_HTTP_ARG_VAL(info, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(putData, 2)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(data, 0)
	PHP_HTTP_ARG_VAL(options, 0)
	PHP_HTTP_ARG_VAL(info, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(putFile, 2)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(file, 0)
	PHP_HTTP_ARG_VAL(options, 0)
	PHP_HTTP_ARG_VAL(info, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(putStream, 2)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(stream, 0)
	PHP_HTTP_ARG_VAL(options, 0)
	PHP_HTTP_ARG_VAL(info, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(methodRegister, 1)
	PHP_HTTP_ARG_VAL(method_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(methodUnregister, 1)
	PHP_HTTP_ARG_VAL(method, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(methodName, 1)
	PHP_HTTP_ARG_VAL(method_id, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(methodExists, 1)
	PHP_HTTP_ARG_VAL(method, 0)
PHP_HTTP_END_ARGS;

#ifdef HAVE_CURL_FORMGET
PHP_HTTP_BEGIN_ARGS(encodeBody, 2)
	PHP_HTTP_ARG_VAL(fields, 0)
	PHP_HTTP_ARG_VAL(files, 0)
PHP_HTTP_END_ARGS;
#endif

zend_class_entry *php_http_request_class_entry;
zend_function_entry php_http_request_method_entry[] = {
	PHP_HTTP_REQUEST_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)

	PHP_HTTP_REQUEST_ME(setOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(setSslOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getSslOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(addSslOptions, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(addHeaders, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getHeaders, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(setHeaders, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(addCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(setCookies, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(enableCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(resetCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(flushCookies, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setMethod, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getMethod, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setUrl, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getUrl, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setContentType, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getContentType, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setQueryData, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getQueryData, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(addQueryData, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setBody, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getBody, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(addBody, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(send, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(getResponseHeader, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseCode, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseStatus, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseBody, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseInfo, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseMessage, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getRequestMessage, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getHistory, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(clearHistory, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(getMessageClass, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(setMessageClass, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers php_http_request_object_handlers;

zend_object_value php_http_request_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_request_object_new_ex(ce, NULL, NULL);
}

zend_object_value php_http_request_object_new_ex(zend_class_entry *ce, CURL *ch, php_http_request_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_request_object_t *o;

	o = ecalloc(1, sizeof(php_http_request_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	o->request = php_http_request_init(NULL, ch, 0, NULL TSRMLS_CC);

	if (ptr) {
		*ptr = o;
	}

	ov.handle = zend_objects_store_put(o, NULL, php_http_request_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_request_object_handlers;

	return ov;
}

zend_object_value php_http_request_object_clone(zval *this_ptr TSRMLS_DC)
{
	zend_object_value new_ov;
	php_http_request_object_t *new_obj, *old_obj = zend_object_store_get_object(this_ptr TSRMLS_CC);

	new_ov = php_http_request_object_new_ex(old_obj->zo.ce, NULL, &new_obj TSRMLS_CC);
	if (old_obj->request->ch) {
		php_http_curl_init(php_http_curl_copy(old_obj->request->ch), new_obj->request TSRMLS_CC);
	}

	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);
	/* FIXME */

	return new_ov;
}

void php_http_request_object_free(void *object TSRMLS_DC)
{
	php_http_request_object_t *o = (php_http_request_object_t *) object;

	php_http_request_free(&o->request);
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

static inline void php_http_request_object_check_request_content_type(zval *this_ptr TSRMLS_DC)
{
	zval *ctype = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("contentType"), 0 TSRMLS_CC);

	if (Z_STRLEN_P(ctype)) {
		zval **headers, *opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);

		if (	(Z_TYPE_P(opts) == IS_ARRAY) &&
				(SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), ZEND_STRS("headers"), (void *) &headers)) &&
				(Z_TYPE_PP(headers) == IS_ARRAY)) {
			zval **ct_header;

			/* only override if not already set */
			if ((SUCCESS != zend_hash_find(Z_ARRVAL_PP(headers), ZEND_STRS("Content-Type"), (void *) &ct_header))) {
				add_assoc_stringl(*headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
			} else
			/* or not a string, zero length string or a string of spaces */
			if ((Z_TYPE_PP(ct_header) != IS_STRING) || !Z_STRLEN_PP(ct_header)) {
				add_assoc_stringl(*headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
			} else {
				int i, only_space = 1;

				/* check for spaces only */
				for (i = 0; i < Z_STRLEN_PP(ct_header); ++i) {
					if (!PHP_HTTP_IS_CTYPE(space, Z_STRVAL_PP(ct_header)[i])) {
						only_space = 0;
						break;
					}
				}
				if (only_space) {
					add_assoc_stringl(*headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
				}
			}
		} else {
			zval *headers;

			MAKE_STD_ZVAL(headers);
			array_init(headers);
			add_assoc_stringl(headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
			zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addheaders", NULL, headers);
			zval_ptr_dtor(&headers);
		}
	}
}

static inline zend_object_value php_http_request_object_message(zval *this_ptr, php_http_message_t *msg TSRMLS_DC)
{
	zend_object_value ov;
	zval *zcn = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("messageClass"), 0 TSRMLS_CC);
	zend_class_entry *class_entry;

	if (Z_STRLEN_P(zcn)
	&&	(class_entry = zend_fetch_class(Z_STRVAL_P(zcn), Z_STRLEN_P(zcn), 0 TSRMLS_CC))
	&&	(SUCCESS == php_http_new(&ov, class_entry, (php_http_new_t) php_http_message_object_new_ex, php_http_message_class_entry, msg, NULL TSRMLS_CC))) {
		return ov;
	} else {
		return php_http_message_object_new_ex(php_http_message_class_entry, msg, NULL TSRMLS_CC);
	}
}

STATUS php_http_request_object_requesthandler(php_http_request_object_t *obj, zval *this_ptr TSRMLS_DC)
{
	STATUS status = SUCCESS;
	zval *zurl, *zmeth, *zbody, *zqdata, *zoptions;
	php_http_message_body_t *body = NULL;
	php_url *tmp, qdu = {0};

	php_http_request_reset(obj->request);
	PHP_HTTP_CHECK_CURL_INIT(obj->request->ch, php_http_curl_init(NULL, obj->request TSRMLS_CC), return FAILURE);
	php_http_request_object_check_request_content_type(getThis() TSRMLS_CC);

	zmeth = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("method"), 0 TSRMLS_CC);
	obj->request->meth = Z_LVAL_P(zmeth);

	zurl = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("url"), 0 TSRMLS_CC);
	zqdata = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), 0 TSRMLS_CC);
	if (Z_STRLEN_P(zqdata)) {
		qdu.query = Z_STRVAL_P(zqdata);
	}
	php_http_url(0, tmp = php_url_parse(Z_STRVAL_P(zurl)), &qdu, NULL, &obj->request->url, NULL TSRMLS_CC);
	php_url_free(tmp);

	zbody = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody"), 0 TSRMLS_CC);
	if (Z_TYPE_P(zbody) == IS_OBJECT) {
		body = ((php_http_message_body_object_t *)zend_object_store_get_object(zbody TSRMLS_CC))->body;
	}
	obj->request->body = body;

	zoptions = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
	php_http_request_prepare(obj->request, Z_ARRVAL_P(zoptions));

	/* check if there's a onProgress method and add it as progress callback if one isn't already set */
	if (zend_hash_exists(&Z_OBJCE_P(getThis())->function_table, ZEND_STRS("onprogress"))) {
		zval **entry, *pcb;

		if ((Z_TYPE_P(zoptions) != IS_ARRAY)
		||	(SUCCESS != zend_hash_find(Z_ARRVAL_P(zoptions), ZEND_STRS("onprogress"), (void *) &entry)
		||	(!zend_is_callable(*entry, 0, NULL TSRMLS_CC)))) {
			MAKE_STD_ZVAL(pcb);
			array_init(pcb);
			Z_ADDREF_P(getThis());
			add_next_index_zval(pcb, getThis());
			add_next_index_stringl(pcb, "onprogress", lenof("onprogress"), 1);
			php_http_request_set_progress_callback(obj->request, pcb);
			zval_ptr_dtor(&pcb);
		}
	}

	return status;
}

STATUS php_http_request_object_responsehandler(php_http_request_object_t *obj, zval *this_ptr TSRMLS_DC)
{
	STATUS ret = SUCCESS;
	zval *info;
	php_http_message_t *msg;

	/* always fetch info */
	MAKE_STD_ZVAL(info);
	array_init(info);
	php_http_request_info(obj->request, Z_ARRVAL_P(info));
	zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseInfo"), info TSRMLS_CC);
	zval_ptr_dtor(&info);

	/* update history * /
	if (i_zend_is_true(zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("recordHistory"), 0 TSRMLS_CC))) {
		zval *new_hist, *old_hist = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("history"), 0 TSRMLS_CC);
		zend_object_value ov = php_http_request_object_message(getThis(), obj->request->message_parser.message TSRMLS_CC);

		MAKE_STD_ZVAL(new_hist);
		ZVAL_OBJVAL(new_hist, ov, 0);

		if (Z_TYPE_P(old_hist) == IS_OBJECT) {
			php_http_message_object_prepend(new_hist, old_hist, 0 TSRMLS_CC);
		}

		zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("history"), new_hist TSRMLS_CC);
		zval_ptr_dtor(&new_hist);
	}
*/
//	if ((msg = obj->request->_current.request)) {
//		/* update request message */
//		zval *message;
//
//		MAKE_STD_ZVAL(message);
//		ZVAL_OBJVAL(message, php_http_request_object_message(getThis(), msg TSRMLS_CC), 1);
//		zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestMessage"), message TSRMLS_CC);
//	}
// fprintf(stderr, "RESPONSE MESSAGE: %p\n", obj->request->parser.msg);
	if ((msg = obj->request->parser.msg)) {
		/* update properties with response info */
		zval *message;

		zend_update_property_long(php_http_request_class_entry, getThis(), ZEND_STRL("responseCode"), msg->http.info.response.code TSRMLS_CC);
		zend_update_property_string(php_http_request_class_entry, getThis(), ZEND_STRL("responseStatus"), STR_PTR(msg->http.info.response.status) TSRMLS_CC);

		MAKE_STD_ZVAL(message);
		ZVAL_OBJVAL(message, php_http_request_object_message(getThis(), msg TSRMLS_CC), 0);
		zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), message TSRMLS_CC);
		zval_ptr_dtor(&message);
		obj->request->parser.msg = php_http_message_init(NULL, 0 TSRMLS_CC);
	} else {
		/* update properties with empty values */
		zval *znull;

		MAKE_STD_ZVAL(znull);
		ZVAL_NULL(znull);
		zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), znull TSRMLS_CC);
		zval_ptr_dtor(&znull);

		zend_update_property_long(php_http_request_class_entry, getThis(), ZEND_STRL("responseCode"), 0 TSRMLS_CC);
		zend_update_property_string(php_http_request_class_entry, getThis(), ZEND_STRL("responseStatus"), "" TSRMLS_CC);
	}

	php_http_request_set_progress_callback(obj->request, NULL);

	if (!EG(exception) && zend_hash_exists(&Z_OBJCE_P(getThis())->function_table, ZEND_STRS("onfinish"))) {
		zval *param;

		MAKE_STD_ZVAL(param);
		ZVAL_BOOL(param, ret == SUCCESS);
		with_error_handling(EH_NORMAL, NULL) {
			zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "onfinish", NULL, param);
		} end_error_handling();
		zval_ptr_dtor(&param);
	}

	return ret;
}

static int apply_pretty_key(void *pDest, int num_args, va_list args, zend_hash_key *hash_key)
{
	if (hash_key->arKey && hash_key->nKeyLength > 1) {
		hash_key->h = zend_hash_func(php_http_pretty_key(hash_key->arKey, hash_key->nKeyLength - 1, 1, 0), hash_key->nKeyLength);
	}
	return ZEND_HASH_APPLY_KEEP;
}

static inline void php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAMETERS, char *key, size_t len, int overwrite, int prettify_keys)
{
	zval *old_opts, *new_opts, *opts = NULL, **entry = NULL;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a/!", &opts)) {
		RETURN_FALSE;
	}

	MAKE_STD_ZVAL(new_opts);
	array_init(new_opts);
	old_opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
	if (Z_TYPE_P(old_opts) == IS_ARRAY) {
		array_copy(Z_ARRVAL_P(old_opts), Z_ARRVAL_P(new_opts));
	}

	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(new_opts), key, len, (void *) &entry)) {
		if (overwrite) {
			zend_hash_clean(Z_ARRVAL_PP(entry));
		}
		if (opts && zend_hash_num_elements(Z_ARRVAL_P(opts))) {
			if (overwrite) {
				array_copy(Z_ARRVAL_P(opts), Z_ARRVAL_PP(entry));
			} else {
				array_join(Z_ARRVAL_P(opts), Z_ARRVAL_PP(entry), 0, prettify_keys ? ARRAY_JOIN_PRETTIFY : 0);
			}
		}
	} else if (opts) {
		if (prettify_keys) {
			zend_hash_apply_with_arguments(Z_ARRVAL_P(opts) TSRMLS_CC, apply_pretty_key, 0, NULL);
		}
		Z_ADDREF_P(opts);
		add_assoc_zval_ex(new_opts, key, len, opts);
	}
	zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), new_opts TSRMLS_CC);
	zval_ptr_dtor(&new_opts);

	RETURN_TRUE;
}

static inline void php_http_request_object_get_options_subr(INTERNAL_FUNCTION_PARAMETERS, char *key, size_t len)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *opts, **options;

		opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
		array_init(return_value);

		if (	(Z_TYPE_P(opts) == IS_ARRAY) &&
				(SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), key, len, (void *) &options))) {
			convert_to_array(*options);
			array_copy(Z_ARRVAL_PP(options), Z_ARRVAL_P(return_value));
		}
	}
}


PHP_METHOD(HttpRequest, __construct)
{
	char *url_str = NULL;
	int url_len;
	long meth = -1;
	zval *options = NULL;

	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sla!", &url_str, &url_len, &meth, &options)) {
			if (url_str) {
				zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("url"), url_str, url_len TSRMLS_CC);
			}
			if (meth > -1) {
				zend_update_property_long(php_http_request_class_entry, getThis(), ZEND_STRL("method"), meth TSRMLS_CC);
			}
			if (options) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "setoptions", NULL, options);
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequest, setOptions)
{
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	HashPosition pos;
	zval *opts = NULL, *old_opts, *new_opts, *add_opts, **opt;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		RETURN_FALSE;
	}

	MAKE_STD_ZVAL(new_opts);
	array_init(new_opts);

	if (!opts || !zend_hash_num_elements(Z_ARRVAL_P(opts))) {
		zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), new_opts TSRMLS_CC);
		zval_ptr_dtor(&new_opts);
		RETURN_TRUE;
	}

	MAKE_STD_ZVAL(add_opts);
	array_init(add_opts);
	/* some options need extra attention -- thus cannot use array_merge() directly */
	FOREACH_KEYVAL(pos, opts, key, opt) {
		if (key.type == HASH_KEY_IS_STRING) {
#define KEYMATCH(k, s) ((sizeof(s)==k.len) && !strcasecmp(k.str, s))
			if (KEYMATCH(key, "headers")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addheaders", NULL, *opt);
			} else if (KEYMATCH(key, "cookies")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addcookies", NULL, *opt);
			} else if (KEYMATCH(key, "ssl")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addssloptions", NULL, *opt);
			} else if (KEYMATCH(key, "url") || KEYMATCH(key, "uri")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "seturl", NULL, *opt);
			} else if (KEYMATCH(key, "method")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "setmethod", NULL, *opt);
			} else if (KEYMATCH(key, "flushcookies")) {
				if (i_zend_is_true(*opt)) {
					php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

					php_http_request_flush_cookies(obj->request);
				}
			} else if (KEYMATCH(key, "resetcookies")) {
				php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				php_http_request_reset_cookies(obj->request, (zend_bool) i_zend_is_true(*opt));
			} else if (KEYMATCH(key, "enablecookies")) {
				php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

				php_http_request_enable_cookies(obj->request);
			} else if (KEYMATCH(key, "recordHistory")) {
				zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("recordHistory"), *opt TSRMLS_CC);
			} else if (KEYMATCH(key, "messageClass")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "setmessageclass", NULL, *opt);
			} else if (Z_TYPE_PP(opt) == IS_NULL) {
				old_opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
				if (Z_TYPE_P(old_opts) == IS_ARRAY) {
					zend_hash_del(Z_ARRVAL_P(old_opts), key.str, key.len);
				}
			} else {
				Z_ADDREF_P(*opt);
				add_assoc_zval_ex(add_opts, key.str, key.len, *opt);
			}
		}
	}

	old_opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
	if (Z_TYPE_P(old_opts) == IS_ARRAY) {
		array_copy(Z_ARRVAL_P(old_opts), Z_ARRVAL_P(new_opts));
	}
	array_join(Z_ARRVAL_P(add_opts), Z_ARRVAL_P(new_opts), 0, 0);
	zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), new_opts TSRMLS_CC);
	zval_ptr_dtor(&new_opts);
	zval_ptr_dtor(&add_opts);

	RETURN_TRUE;
}



PHP_METHOD(HttpRequest, getOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "options");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setSslOptions)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("ssl"), 1, 0);
}

PHP_METHOD(HttpRequest, addSslOptions)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("ssl"), 0, 0);
}

PHP_METHOD(HttpRequest, getSslOptions)
{
	php_http_request_object_get_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("ssl"));
}

PHP_METHOD(HttpRequest, addHeaders)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("headers"), 0, 1);
}

PHP_METHOD(HttpRequest, setHeaders)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("headers"), 1, 1);
}

PHP_METHOD(HttpRequest, getHeaders)
{
	php_http_request_object_get_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("headers"));
}

PHP_METHOD(HttpRequest, setCookies)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("cookies"), 1, 0);
}

PHP_METHOD(HttpRequest, addCookies)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("cookies"), 0, 0);
}

PHP_METHOD(HttpRequest, getCookies)
{
	php_http_request_object_get_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("cookies"));
}

PHP_METHOD(HttpRequest, enableCookies)
{
	if (SUCCESS == zend_parse_parameters_none()){
		php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_SUCCESS(php_http_request_enable_cookies(obj->request));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, resetCookies)
{
	zend_bool session_only = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &session_only)) {
		php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		RETURN_SUCCESS(php_http_request_reset_cookies(obj->request, session_only));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, flushCookies)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_SUCCESS(php_http_request_flush_cookies(obj->request));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setUrl)
{
	char *url_str = NULL;
	int url_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &url_len, &url_len)) {
		zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("url"), url_str, url_len TSRMLS_CC);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getUrl)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "url");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setMethod)
{
	long meth;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &meth)) {
		zend_update_property_long(php_http_request_class_entry, getThis(), ZEND_STRL("method"), meth TSRMLS_CC);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getMethod)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "method");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setContentType)
{
	char *ctype;
	int ct_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ct_len)) {
		if (ct_len) {
			PHP_HTTP_CHECK_CONTENT_TYPE(ctype, RETURN_FALSE);
		}
		zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("contentType"), ctype, ct_len TSRMLS_CC);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getContentType)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "contentType");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setQueryData)
{
	zval *qdata = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z!", &qdata)) {
		if ((!qdata) || Z_TYPE_P(qdata) == IS_NULL) {
			zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), "", 0 TSRMLS_CC);
		} else if ((Z_TYPE_P(qdata) == IS_ARRAY) || (Z_TYPE_P(qdata) == IS_OBJECT)) {
			char *query_data_str = NULL;
			size_t query_data_len;

			if (SUCCESS != php_http_url_encode_hash(HASH_OF(qdata), 0, NULL, 0, &query_data_str, &query_data_len TSRMLS_CC)) {
				RETURN_FALSE;
			}

			zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), query_data_str, query_data_len TSRMLS_CC);
			efree(query_data_str);
		} else {
			zval *data = php_http_zsep(IS_STRING, qdata);

			zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), Z_STRVAL_P(data), Z_STRLEN_P(data) TSRMLS_CC);
			zval_ptr_dtor(&data);
		}
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getQueryData)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "queryData");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, addQueryData)
{
	zval *qdata;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &qdata)) {
		char *query_data_str = NULL;
		size_t query_data_len = 0;
		zval *old_qdata = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), 0 TSRMLS_CC);

		if (SUCCESS != php_http_url_encode_hash(HASH_OF(qdata), 1, Z_STRVAL_P(old_qdata), Z_STRLEN_P(old_qdata), &query_data_str, &query_data_len TSRMLS_CC)) {
			RETURN_FALSE;
		}

		zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), query_data_str, query_data_len TSRMLS_CC);
		efree(query_data_str);

		RETURN_TRUE;
	}
	RETURN_FALSE;

}

PHP_METHOD(HttpRequest, setBody)
{
	zval *body = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|O!", &body, php_http_message_body_class_entry)) {
		if (body && Z_TYPE_P(body) != IS_NULL) {
			zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody"), body TSRMLS_CC);
		} else {
			zend_update_property_null(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody") TSRMLS_CC);
		}
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, addBody)
{
	zval *new_body;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &new_body, php_http_message_body_class_entry)) {
		zval *old_body = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody"), 0 TSRMLS_CC);

		if (Z_TYPE_P(old_body) == IS_OBJECT) {
			php_http_message_body_object_t *old_obj = zend_object_store_get_object(old_body TSRMLS_CC);
			php_http_message_body_object_t *new_obj = zend_object_store_get_object(new_body TSRMLS_CC);

			php_http_message_body_to_callback(old_obj->body, (php_http_pass_callback_t) php_http_message_body_append, new_obj->body, 0, 0);
		} else {
			zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody"), new_body TSRMLS_CC);
		}
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getBody)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "requestBody");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseHeader)
{
	zval *header;
	char *header_name = NULL;
	int header_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &header_name, &header_len)) {
		zval *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), 0 TSRMLS_CC);

		if (Z_TYPE_P(message) == IS_OBJECT) {
			php_http_message_object_t *msg = zend_object_store_get_object(message TSRMLS_CC);

			if (header_len) {
				if ((header = php_http_message_header(msg->message, header_name, header_len + 1, 0))) {
					RETURN_ZVAL(header, 1, 1);
				}
			} else {
				array_init(return_value);
				zend_hash_copy(Z_ARRVAL_P(return_value), &msg->message->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
				return;
			}
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseCookies)
{
	long flags = 0;
	zval *allowed_extras_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|la!", &flags, &allowed_extras_array)) {
		int i = 0;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
		char **allowed_extras = NULL;
		zval **header = NULL, **entry = NULL, *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), 0 TSRMLS_CC);
		HashPosition pos, pos1, pos2;

		if (Z_TYPE_P(message) == IS_OBJECT) {
			php_http_message_object_t *msg = zend_object_store_get_object(message TSRMLS_CC);

			array_init(return_value);

			if (allowed_extras_array) {
				allowed_extras = ecalloc(zend_hash_num_elements(Z_ARRVAL_P(allowed_extras_array)) + 1, sizeof(char *));
				FOREACH_VAL(pos, allowed_extras_array, entry) {
					zval *data = php_http_zsep(IS_STRING, *entry);
					allowed_extras[i++] = estrndup(Z_STRVAL_P(data), Z_STRLEN_P(data));
					zval_ptr_dtor(&data);
				}
			}

			FOREACH_HASH_KEYVAL(pos1, &msg->message->hdrs, key, header) {
				if (key.type == HASH_KEY_IS_STRING && !strcasecmp(key.str, "Set-Cookie")) {
					php_http_cookie_list_t *list;

					if (Z_TYPE_PP(header) == IS_ARRAY) {
						zval **single_header;

						FOREACH_VAL(pos2, *header, single_header) {
							zval *data = php_http_zsep(IS_STRING, *single_header);

							if ((list = php_http_cookie_list_parse(NULL, Z_STRVAL_P(data), flags, allowed_extras TSRMLS_CC))) {
								zval *cookie;

								MAKE_STD_ZVAL(cookie);
								ZVAL_OBJVAL(cookie, php_http_cookie_object_new_ex(php_http_cookie_class_entry, list, NULL TSRMLS_CC), 0);
								add_next_index_zval(return_value, cookie);
							}
							zval_ptr_dtor(&data);
						}
					} else {
						zval *data = php_http_zsep(IS_STRING, *header);
						if ((list = php_http_cookie_list_parse(NULL, Z_STRVAL_P(data), flags, allowed_extras TSRMLS_CC))) {
							zval *cookie;

							MAKE_STD_ZVAL(cookie);
							ZVAL_OBJVAL(cookie, php_http_cookie_object_new_ex(php_http_cookie_class_entry, list, NULL TSRMLS_CC), 0);
							add_next_index_zval(return_value, cookie);
						}
						zval_ptr_dtor(&data);
					}
				}
			}

			if (allowed_extras) {
				for (i = 0; allowed_extras[i]; ++i) {
					efree(allowed_extras[i]);
				}
				efree(allowed_extras);
			}

			return;
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseBody)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), 0 TSRMLS_CC);

		RETURN_OBJVAL(((php_http_message_object_t *)zend_object_store_get_object(message TSRMLS_CC))->body, 1);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseCode)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "responseCode");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseStatus)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "responseStatus");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseInfo)
{
	char *info_name = NULL;
	int info_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &info_name, &info_len)) {
		zval **infop, *info = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseInfo"), 0 TSRMLS_CC);

		if (Z_TYPE_P(info) != IS_ARRAY) {
			RETURN_FALSE;
		}

		if (info_len && info_name) {
			if (SUCCESS == zend_hash_find(Z_ARRVAL_P(info), php_http_pretty_key(info_name, info_len, 0, 0), info_len + 1, (void *) &infop)) {
				RETURN_ZVAL(*infop, 1, 0);
			} else {
				php_http_error(HE_NOTICE, PHP_HTTP_E_INVALID_PARAM, "Could not find response info named %s", info_name);
				RETURN_FALSE;
			}
		} else {
			RETURN_ZVAL(info, 1, 0);
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseMessage)
{
	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), 0 TSRMLS_CC);

			if (Z_TYPE_P(message) == IS_OBJECT) {
				RETVAL_OBJECT(message, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "HttpRequest does not contain a response message");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequest, getRequestMessage)
{
	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestMessage"), 0 TSRMLS_CC);

			if (Z_TYPE_P(message) == IS_OBJECT) {
				RETVAL_OBJECT(message, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "HttpRequest does not contain a request message");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequest, getHistory)
{
	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *hist = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("history"), 0 TSRMLS_CC);

			if (Z_TYPE_P(hist) == IS_OBJECT) {
				RETVAL_OBJECT(hist, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "The history is empty");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequest, clearHistory)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zend_update_property_null(php_http_request_class_entry, getThis(), ZEND_STRL("history") TSRMLS_CC);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getMessageClass)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "messageClass");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setMessageClass)
{
	char *cn;
	int cl;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &cn, &cl)) {
		zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("messageClass"), cn, cl TSRMLS_CC);
	}
}

PHP_METHOD(HttpRequest, send)
{
	RETVAL_FALSE;

	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			if (obj->pool) {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "Cannot perform HttpRequest::send() while attached to an HttpRequestPool");
			} else if (SUCCESS == php_http_request_object_requesthandler(obj, getThis() TSRMLS_CC)) {
				php_http_request_exec(obj->request);
				if (SUCCESS == php_http_request_object_responsehandler(obj, getThis() TSRMLS_CC)) {
					RETVAL_PROP(php_http_request_class_entry, "responseMessage");
				} else {
					php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Failed to handle response");
				}
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Failed to handle request");
			}
		}
	} end_error_handling();
}

PHP_MINIT_FUNCTION(http_request)
{
#ifdef PHP_HTTP_NEED_OPENSSL_TSL
	/* mod_ssl, libpq or ext/curl might already have set thread lock callbacks */
	if (!CRYPTO_get_id_callback()) {
		int i, c = CRYPTO_num_locks();

		php_http_openssl_tsl = malloc(c * sizeof(MUTEX_T));

		for (i = 0; i < c; ++i) {
			php_http_openssl_tsl[i] = tsrm_mutex_alloc();
		}

		CRYPTO_set_id_callback(php_http_openssl_thread_id);
		CRYPTO_set_locking_callback(php_http_openssl_thread_lock);
	}
#endif
#ifdef PHP_HTTP_NEED_GNUTLS_TSL
	gcry_control(GCRYCTL_SET_THREAD_CBS, &php_http_gnutls_tsl);
#endif

	if (CURLE_OK != curl_global_init(CURL_GLOBAL_ALL)) {
		return FAILURE;
	}

	if (SUCCESS != php_http_persistent_handle_provide(ZEND_STRL("http_request"), safe_curl_init, safe_curl_dtor, safe_curl_copy)) {
		return FAILURE;
	}

	PHP_HTTP_REGISTER_CLASS(http, Request, http_request, php_http_object_class_entry, 0);
	php_http_request_class_entry->create_object = php_http_request_object_new;
	memcpy(&php_http_request_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_request_object_handlers.clone_obj = php_http_request_object_clone;

	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("options"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("responseInfo"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("responseMessage"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_long(php_http_request_class_entry, ZEND_STRL("responseCode"), 0, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("responseStatus"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("requestMessage"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_long(php_http_request_class_entry, ZEND_STRL("method"), PHP_HTTP_GET, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("url"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("contentType"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("requestBody"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("queryData"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("history"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_bool(php_http_request_class_entry, ZEND_STRL("recordHistory"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("messageClass"), "", ZEND_ACC_PRIVATE TSRMLS_CC);

	/*
	* HTTP Protocol Version Constants
	*/
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("VERSION_1_0"), CURL_HTTP_VERSION_1_0 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("VERSION_1_1"), CURL_HTTP_VERSION_1_1 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("VERSION_NONE"), CURL_HTTP_VERSION_NONE TSRMLS_CC); /* to be removed */
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("VERSION_ANY"), CURL_HTTP_VERSION_NONE TSRMLS_CC);

	/*
	* SSL Version Constants
	*/
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("SSL_VERSION_TLSv1"), CURL_SSLVERSION_TLSv1 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("SSL_VERSION_SSLv2"), CURL_SSLVERSION_SSLv2 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("SSL_VERSION_SSLv3"), CURL_SSLVERSION_SSLv3 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("SSL_VERSION_ANY"), CURL_SSLVERSION_DEFAULT TSRMLS_CC);

	/*
	* DNS IPvX resolving
	*/
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("IPRESOLVE_V4"), CURL_IPRESOLVE_V4 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("IPRESOLVE_V6"), CURL_IPRESOLVE_V6 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("IPRESOLVE_ANY"), CURL_IPRESOLVE_WHATEVER TSRMLS_CC);

	/*
	* Auth Constants
	*/
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("AUTH_BASIC"), CURLAUTH_BASIC TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("AUTH_DIGEST"), CURLAUTH_DIGEST TSRMLS_CC);
#if PHP_HTTP_CURL_VERSION(7,19,3)
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("AUTH_DIGEST_IE"), CURLAUTH_DIGEST_IE TSRMLS_CC);
#endif
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("AUTH_NTLM"), CURLAUTH_NTLM TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("AUTH_GSSNEG"), CURLAUTH_GSSNEGOTIATE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("AUTH_ANY"), CURLAUTH_ANY TSRMLS_CC);

	/*
	* Proxy Type Constants
	*/
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("PROXY_SOCKS4"), CURLPROXY_SOCKS4 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("PROXY_SOCKS4A"), CURLPROXY_SOCKS5 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("PROXY_SOCKS5_HOSTNAME"), CURLPROXY_SOCKS5 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("PROXY_SOCKS5"), CURLPROXY_SOCKS5 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("PROXY_HTTP"), CURLPROXY_HTTP TSRMLS_CC);
#	if PHP_HTTP_CURL_VERSION(7,19,4)
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("PROXY_HTTP_1_0"), CURLPROXY_HTTP_1_0 TSRMLS_CC);
#	endif

	/*
	* Post Redirection Constants
	*/
#if PHP_HTTP_CURL_VERSION(7,19,1)
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("POSTREDIR_301"), CURL_REDIR_POST_301 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("POSTREDIR_302"), CURL_REDIR_POST_302 TSRMLS_CC);
	zend_declare_class_constant_long(php_http_request_class_entry, ZEND_STRL("POSTREDIR_ALL"), CURL_REDIR_POST_ALL TSRMLS_CC);
#endif

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_request)
{
	curl_global_cleanup();
#ifdef PHP_HTTP_NEED_OPENSSL_TSL
	if (php_http_openssl_tsl) {
		int i, c = CRYPTO_num_locks();

		CRYPTO_set_id_callback(NULL);
		CRYPTO_set_locking_callback(NULL);

		for (i = 0; i < c; ++i) {
			tsrm_mutex_free(php_http_openssl_tsl[i]);
		}

		free(php_http_openssl_tsl);
		php_http_openssl_tsl = NULL;
	}
#endif
	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

