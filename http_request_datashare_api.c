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

#define HTTP_WANT_CURL
#include "php_http.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)

#include "php_http_api.h"
#include "php_http_request_datashare_api.h"
#include "php_http_request_api.h"
#include "php_http_request_object.h"

#ifndef HAVE_CURL_SHARE_STRERROR
#	define curl_share_strerror(dummy) "unknown error"
#endif

static HashTable http_request_datashare_options;
static http_request_datashare http_request_datashare_global;
static int http_request_datashare_compare_handles(void *h1, void *h2);
static void http_request_datashare_lock_func(CURL *handle, curl_lock_data data, curl_lock_access locktype, void *userptr);
static void http_request_datashare_unlock_func(CURL *handle, curl_lock_data data, void *userptr);

http_request_datashare *_http_request_datashare_global_get(void)
{
	return &http_request_datashare_global;
}

PHP_MINIT_FUNCTION(http_request_datashare)
{
	curl_lock_data val;
	
	zend_hash_init(&http_request_datashare_options, 4, NULL, NULL, 1);
#define ADD_DATASHARE_OPT(name, opt) \
	val = opt; \
	zend_hash_add(&http_request_datashare_options, name, sizeof(name), &val, sizeof(curl_lock_data), NULL)
	ADD_DATASHARE_OPT("cookie", CURL_LOCK_DATA_COOKIE);
	ADD_DATASHARE_OPT("dns", CURL_LOCK_DATA_DNS);
	ADD_DATASHARE_OPT("ssl", CURL_LOCK_DATA_SSL_SESSION);
	ADD_DATASHARE_OPT("connect", CURL_LOCK_DATA_CONNECT);
	
	http_request_datashare_init_ex(&http_request_datashare_global, 1);
	
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_request_datashare)
{
	http_request_datashare_dtor(&http_request_datashare_global);
	zend_hash_destroy(&http_request_datashare_options);
	
	return SUCCESS;
}

PHP_HTTP_API http_request_datashare *_http_request_datashare_init_ex(http_request_datashare *share, zend_bool persistent TSRMLS_DC)
{
	int i;
	zend_bool free_share;
	
	if ((free_share = !share)) {
		share = pemalloc(sizeof(http_request_datashare), persistent);
	}
	memset(share, 0, sizeof(http_request_datashare));
	
	HTTP_CHECK_CURL_INIT(share->ch, curl_share_init(), ;);
	if (!share->ch) {
		if (free_share) {
			pefree(share, persistent);
		}
		return NULL;
	}
	
	if ((share->persistent = persistent)) {
		share->locks = pecalloc(CURL_LOCK_DATA_LAST, sizeof(http_request_datashare_lock), 1);
		for (i = 0; i < CURL_LOCK_DATA_LAST; ++i) {
			share->locks[i].mx = tsrm_mutex_alloc();
		}
		curl_share_setopt(share->ch, CURLSHOPT_LOCKFUNC, http_request_datashare_lock_func);
		curl_share_setopt(share->ch, CURLSHOPT_UNLOCKFUNC, http_request_datashare_unlock_func);
		curl_share_setopt(share->ch, CURLSHOPT_USERDATA, share);
	}
	
	zend_llist_init(&share->handles, sizeof(zval *), ZVAL_PTR_DTOR, persistent);
	
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
	
	if (CURLE_OK != (rc = curl_easy_setopt(obj->request->ch, CURLOPT_SHARE, share->ch))) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "Could not attach HttpRequest object(#%d) to the HttpRequestDataShare: %s", Z_OBJ_HANDLE_P(request), curl_share_strerror(rc));
		return FAILURE;
	}
	
	obj->share = share;
	ZVAL_ADDREF(request);
	zend_llist_add_element(&share->handles, (void *) &request);
	
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
		zend_llist_del_element(&share->handles, request, http_request_datashare_compare_handles);
		return SUCCESS;
	}
	return FAILURE;
}

PHP_HTTP_API void _http_request_datashare_detach_all(http_request_datashare *share TSRMLS_DC)
{
	zval **r;
	
	while ((r = zend_llist_get_first(&share->handles))) {
		http_request_datashare_detach(share, *r);
	}
}

PHP_HTTP_API void _http_request_datashare_dtor(http_request_datashare *share TSRMLS_DC)
{
	int i;
	
	zend_llist_destroy(&share->handles);
	curl_share_cleanup(share->ch);
	if (share->persistent) {
		for (i = 0; i < CURL_LOCK_DATA_LAST; ++i) {
			tsrm_mutex_free(share->locks[i].mx);
		}
		pefree(share->locks, 1);
	}
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

static void http_request_datashare_lock_func(CURL *handle, curl_lock_data data, curl_lock_access locktype, void *userptr)
{
	http_request_datashare *share = (http_request_datashare *) userptr;
	
	/* TSRM can't distinguish shared/exclusive locks */
	tsrm_mutex_lock(share->locks[data].mx);
	share->locks[data].ch = handle;
}

static void http_request_datashare_unlock_func(CURL *handle, curl_lock_data data, void *userptr)
{
	http_request_datashare *share = (http_request_datashare *) userptr;
	
	if (share->locks[data].ch == handle) {
		tsrm_mutex_unlock(share->locks[data].mx);
	}
}

#endif /* ZEND_ENGINE_2 && HTTP_HAVE_CURL */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

