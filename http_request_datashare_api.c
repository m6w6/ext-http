/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_CURL
#include "php_http.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)

#include "php_http_api.h"
#include "php_http_request_datashare_api.h"
#include "php_http_request_api.h"
#include "php_http_request_object.h"
#ifdef HTTP_HAVE_PERSISTENT_HANDLES
#	include "php_http_persistent_handle_api.h"
#endif

#ifdef HTTP_HAVE_PERSISTENT_HANDLES
#	define HTTP_CURL_SHARE_CTOR(ch) (SUCCESS == http_persistent_handle_acquire("http_request_datashare", &(ch)))
#	define HTTP_CURL_SHARE_DTOR(chp) http_persistent_handle_release("http_request_datashare", (chp))
#	define HTTP_CURL_SLOCK_CTOR(l) (SUCCESS == http_persistent_handle_acquire("http_request_datashare_lock", (void *) &(l)))
#	define HTTP_CURL_SLOCK_DTOR(lp) http_persistent_handle_release("http_request_datashare_lock", (void *) (lp))
#else
#	define HTTP_CURL_SHARE_CTOR(ch) ((ch) = curl_share_init())
#	define HTTP_CURL_SHARE_DTOR(chp) curl_share_cleanup(*(chp)); *(chp) = NULL
#	define HTTP_CURL_SLOCK_CTOR(l) ((l) = http_request_datashare_locks_init())
#	define HTTP_CURL_SLOCK_DTOR(lp) http_request_datashare_locks_dtor(*(lp)); *(lp) = NULL
#endif

static HashTable http_request_datashare_options;
static http_request_datashare http_request_datashare_global;
static int http_request_datashare_compare_handles(void *h1, void *h2);
static void http_request_datashare_destroy_handles(void *el);
#ifdef ZTS
static void *http_request_datashare_locks_init(void);
static void http_request_datashare_locks_dtor(void *l);
static void http_request_datashare_lock_func(CURL *handle, curl_lock_data data, curl_lock_access locktype, void *userptr);
static void http_request_datashare_unlock_func(CURL *handle, curl_lock_data data, void *userptr);
#endif

http_request_datashare *_http_request_datashare_global_get(void)
{
	return &http_request_datashare_global;
}

PHP_MINIT_FUNCTION(http_request_datashare)
{
	curl_lock_data val;
	
#ifdef HTTP_HAVE_PERSISTENT_HANDLES
	if (SUCCESS != http_persistent_handle_provide("http_request_datashare", curl_share_init, (http_persistent_handle_dtor) curl_share_cleanup)) {
		return FAILURE;
	}
#	ifdef ZTS
	if (SUCCESS != http_persistent_handle_provide("http_request_datashare_lock", http_request_datashare_locks_init, http_request_datashare_locks_dtor)) {
		return FAILURE;
	}
#	endif
#endif
	
	if (!http_request_datashare_init_ex(&http_request_datashare_global, 1)) {
		return FAILURE;
	}
	
	zend_hash_init(&http_request_datashare_options, 4, NULL, NULL, 1);
#define ADD_DATASHARE_OPT(name, opt) \
	val = opt; \
	zend_hash_add(&http_request_datashare_options, name, sizeof(name), &val, sizeof(curl_lock_data), NULL)
	ADD_DATASHARE_OPT("cookie", CURL_LOCK_DATA_COOKIE);
	ADD_DATASHARE_OPT("dns", CURL_LOCK_DATA_DNS);
	ADD_DATASHARE_OPT("ssl", CURL_LOCK_DATA_SSL_SESSION);
	ADD_DATASHARE_OPT("connect", CURL_LOCK_DATA_CONNECT);
	
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_request_datashare)
{
	http_request_datashare_dtor(&http_request_datashare_global);
	zend_hash_destroy(&http_request_datashare_options);
	
	return SUCCESS;
}

PHP_RINIT_FUNCTION(http_request_datashare)
{
	zend_llist_init(&HTTP_G->request.datashare.handles, sizeof(zval *), http_request_datashare_destroy_handles, 0);
	
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http_request_datashare)
{
	zend_llist_destroy(&HTTP_G->request.datashare.handles);
	
	return SUCCESS;
}

PHP_HTTP_API http_request_datashare *_http_request_datashare_init_ex(http_request_datashare *share, zend_bool persistent TSRMLS_DC)
{
	zend_bool free_share;
	
	if ((free_share = !share)) {
		share = pemalloc(sizeof(http_request_datashare), persistent);
	}
	memset(share, 0, sizeof(http_request_datashare));
	
	if (!HTTP_CURL_SHARE_CTOR(share->ch)) {
		if (free_share) {
			pefree(share, persistent);
		}
		return NULL;
	}
	
	if (!(share->persistent = persistent)) {
		share->handle.list = emalloc(sizeof(zend_llist));
		zend_llist_init(share->handle.list, sizeof(zval *), ZVAL_PTR_DTOR, 0);
#ifdef ZTS
	} else {
		if (HTTP_CURL_SLOCK_CTOR(share->handle.locks)) {
			curl_share_setopt(share->ch, CURLSHOPT_LOCKFUNC, http_request_datashare_lock_func);
			curl_share_setopt(share->ch, CURLSHOPT_UNLOCKFUNC, http_request_datashare_unlock_func);
			curl_share_setopt(share->ch, CURLSHOPT_USERDATA, share);
		}
#endif
	}
	
	return share;
}

PHP_HTTP_API STATUS _http_request_datashare_attach(http_request_datashare *share, zval *request TSRMLS_DC)
{
	CURLcode rc;
	getObjectEx(http_request_object, obj, request);
	
	if (obj->share) {
		if (obj->share == share)  {
			return SUCCESS;
		} else if (SUCCESS != http_request_datashare_detach(obj->share, request)) {
			return FAILURE;
		}
	}
	
	HTTP_CHECK_CURL_INIT(obj->request->ch, http_curl_init_ex(obj->request->ch, obj->request), return FAILURE);
	if (CURLE_OK != (rc = curl_easy_setopt(obj->request->ch, CURLOPT_SHARE, share->ch))) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "Could not attach HttpRequest object(#%d) to the HttpRequestDataShare: %s", Z_OBJ_HANDLE_P(request), curl_easy_strerror(rc));
		return FAILURE;
	}
	
	obj->share = share;
	ZVAL_ADDREF(request);
	zend_llist_add_element(HTTP_RSHARE_HANDLES(share), (void *) &request);
	
	return SUCCESS;
}

PHP_HTTP_API STATUS _http_request_datashare_detach(http_request_datashare *share, zval *request TSRMLS_DC)
{
	CURLcode rc;
	getObjectEx(http_request_object, obj, request);
	
	if (!obj->share) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "HttpRequest object(#%d) is not attached to any HttpRequestDataShare", Z_OBJ_HANDLE_P(request));
	} else if (obj->share != share) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "HttpRequest object(#%d) is not attached to this HttpRequestDataShare", Z_OBJ_HANDLE_P(request));
	} else if (CURLE_OK != (rc = curl_easy_setopt(obj->request->ch, CURLOPT_SHARE, NULL))) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "Could not detach HttpRequest object(#%d) from the HttpRequestDataShare: %s", Z_OBJ_HANDLE_P(request), curl_share_strerror(rc));
	} else {
		obj->share = NULL;
		zend_llist_del_element(HTTP_RSHARE_HANDLES(share), request, http_request_datashare_compare_handles);
		return SUCCESS;
	}
	return FAILURE;
}

PHP_HTTP_API void _http_request_datashare_detach_all(http_request_datashare *share TSRMLS_DC)
{
	zval **r;
	
	while ((r = zend_llist_get_first(HTTP_RSHARE_HANDLES(share)))) {
		http_request_datashare_detach(share, *r);
	}
}

PHP_HTTP_API void _http_request_datashare_dtor(http_request_datashare *share TSRMLS_DC)
{
	if (!share->persistent) {
		zend_llist_destroy(share->handle.list);
		efree(share->handle.list);
	}
	HTTP_CURL_SHARE_DTOR(&share->ch);
#ifdef ZTS
	if (share->persistent) {
		HTTP_CURL_SLOCK_DTOR(&share->handle.locks);
	}
#endif
}

PHP_HTTP_API void _http_request_datashare_free(http_request_datashare **share TSRMLS_DC)
{
	http_request_datashare_dtor(*share);
	pefree(*share, (*share)->persistent);
	*share = NULL;
}

PHP_HTTP_API STATUS _http_request_datashare_set(http_request_datashare *share, const char *option, size_t option_len, zend_bool enable TSRMLS_DC)
{
	curl_lock_data *opt;
	CURLSHcode rc;
	
	if (SUCCESS == zend_hash_find(&http_request_datashare_options, (char *) option, option_len + 1, (void *) &opt)) {
		if (CURLSHE_OK == (rc = curl_share_setopt(share->ch, enable ? CURLSHOPT_SHARE : CURLSHOPT_UNSHARE, *opt))) {
			return SUCCESS;
		}
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "Could not %s sharing of %s data: %s",  enable ? "enable" : "disable", option, curl_share_strerror(rc));
	}
	return FAILURE;
}

static int http_request_datashare_compare_handles(void *h1, void *h2)
{
	return (Z_OBJ_HANDLE_PP((zval **) h1) == Z_OBJ_HANDLE_P((zval *) h2));
}

static void http_request_datashare_destroy_handles(void *el)
{
	zval **r = (zval **) el;
	TSRMLS_FETCH();
	
	{ /* gcc 2.95 needs these braces */
		getObjectEx(http_request_object, obj, *r);
		
		curl_easy_setopt(obj->request->ch, CURLOPT_SHARE, NULL);
		zval_ptr_dtor(r);
	}
}

#ifdef ZTS
static void *http_request_datashare_locks_init(void)
{
	int i;
	http_request_datashare_lock *locks = pecalloc(CURL_LOCK_DATA_LAST, sizeof(http_request_datashare_lock), 1);
	
	if (locks) {
		for (i = 0; i < CURL_LOCK_DATA_LAST; ++i) {
			locks[i].mx = tsrm_mutex_alloc();
		}
	}
	
	return locks;
}

static void http_request_datashare_locks_dtor(void *l)
{
	int i;
	http_request_datashare_lock *locks = (http_request_datashare_lock *) l;
	
	for (i = 0; i < CURL_LOCK_DATA_LAST; ++i) {
		tsrm_mutex_free(locks[i].mx);
	}
	pefree(locks, 1);
}

static void http_request_datashare_lock_func(CURL *handle, curl_lock_data data, curl_lock_access locktype, void *userptr)
{
	http_request_datashare *share = (http_request_datashare *) userptr;
	
	/* TSRM can't distinguish shared/exclusive locks */
	tsrm_mutex_lock(share->handle.locks[data].mx);
	share->handle.locks[data].ch = handle;
}

static void http_request_datashare_unlock_func(CURL *handle, curl_lock_data data, void *userptr)
{
	http_request_datashare *share = (http_request_datashare *) userptr;
	
	if (share->handle.locks[data].ch == handle) {
		tsrm_mutex_unlock(share->handle.locks[data].mx);
	}
}
#endif

#endif /* ZEND_ENGINE_2 && HTTP_HAVE_CURL */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

