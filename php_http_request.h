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

/* $Id: php_http_request_api.h 292841 2009-12-31 08:48:57Z mike $ */

#ifndef PHP_HTTP_REQUEST_H
#define PHP_HTTP_REQUEST_H

#include <curl/curl.h>

#include "php_http_request_method.h"
#include "php_http_request_pool.h"

extern PHP_MINIT_FUNCTION(http_request);
extern PHP_MSHUTDOWN_FUNCTION(http_request);

typedef struct php_http_request_progress_state_counter {
	double now;
	double total;
} php_http_request_progress_state_counter_t;

typedef struct php_http_request_progress_state {
	php_http_request_progress_state_counter_t ul;
	php_http_request_progress_state_counter_t dl;
} php_http_request_progress_state_t;

typedef struct php_http_request {
	CURL *ch;
	char *url;
	php_http_request_method_t meth;
	php_http_message_body_t *body;
	struct {
		php_http_message_parser_t *ctx;
		php_http_message_t *msg;
		php_http_buffer_t *buf;
	} parser;
	
	struct {
		php_http_buffer_t cookies;
		HashTable options;
		struct curl_slist *headers;
		long redirects;
	} _cache;
	
	struct {
		uint count;
		double delay;
	} _retry;

	struct {
		struct {
			struct {
				double now;
				double total;
			} ul;
			struct {
				double now;
				double total;
			} dl;
		} state;
		zval *callback;
		unsigned in_cb:1;
	} _progress;

#ifdef ZTS
	void ***ts;
#endif
	
} php_http_request_t;

/* CURLOPT_PRIVATE storage living as long as a CURL handle */
typedef struct php_http_request_storage {
	char *url;
	char *cookiestore;
	char errorbuffer[CURL_ERROR_SIZE];
} php_http_request_storage_t;


static inline php_http_request_storage_t *php_http_request_storage_get(CURL *ch)
{
	php_http_request_storage_t *st = NULL;
	curl_easy_getinfo(ch, CURLINFO_PRIVATE, &st);
	return st;
}

PHP_HTTP_API CURL *php_http_curl_init(CURL *ch, php_http_request_t *request TSRMLS_DC);
PHP_HTTP_API void php_http_curl_free(CURL **ch TSRMLS_DC);
PHP_HTTP_API CURL *php_http_curl_copy(CURL *ch TSRMLS_DC);

#define PHP_HTTP_CHECK_CURL_INIT(ch, init, action) \
	if ((!(ch)) && (!((ch) = init))) { \
		php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Could not initialize curl"); \
		action; \
	}


PHP_HTTP_API php_http_request_t *php_http_request_init(php_http_request_t *request, CURL *ch, php_http_request_method_t meth, const char *url TSRMLS_DC);
PHP_HTTP_API void php_http_request_dtor(php_http_request_t *request);
PHP_HTTP_API void php_http_request_free(php_http_request_t **request);
PHP_HTTP_API void php_http_request_reset(php_http_request_t *r);
PHP_HTTP_API STATUS php_http_request_enable_cookies(php_http_request_t *request);
PHP_HTTP_API STATUS php_http_request_reset_cookies(php_http_request_t *request, int session_only);
PHP_HTTP_API STATUS php_http_request_flush_cookies(php_http_request_t *request);
PHP_HTTP_API void php_http_request_defaults(php_http_request_t *request);
PHP_HTTP_API STATUS php_http_request_prepare(php_http_request_t *request, HashTable *options);
PHP_HTTP_API void php_http_request_exec(php_http_request_t *request);
PHP_HTTP_API void php_http_request_info(php_http_request_t *request, HashTable *info);
PHP_HTTP_API void php_http_request_set_progress_callback(php_http_request_t *request, zval *cb);


typedef struct php_http_request_object {
	zend_object zo;
	php_http_request_t *request;
	php_http_request_pool_t *pool;
	php_http_request_datashare_t *share;
} php_http_request_object_t;

extern zend_class_entry *php_http_request_class_entry;
extern zend_function_entry php_http_request_method_entry[];

extern zend_object_value php_http_request_object_new(zend_class_entry *ce TSRMLS_DC);
extern zend_object_value php_http_request_object_new_ex(zend_class_entry *ce, CURL *ch, php_http_request_object_t **ptr TSRMLS_DC);
extern zend_object_value php_http_request_object_clone(zval *zobject TSRMLS_DC);
extern void php_http_request_object_free(void *object TSRMLS_DC);

extern STATUS php_http_request_object_requesthandler(php_http_request_object_t *obj, zval *this_ptr TSRMLS_DC);
extern STATUS php_http_request_object_responsehandler(php_http_request_object_t *obj, zval *this_ptr TSRMLS_DC);

PHP_METHOD(HttpRequest, __construct);
PHP_METHOD(HttpRequest, setOptions);
PHP_METHOD(HttpRequest, getOptions);
PHP_METHOD(HttpRequest, addSslOptions);
PHP_METHOD(HttpRequest, setSslOptions);
PHP_METHOD(HttpRequest, getSslOptions);
PHP_METHOD(HttpRequest, addHeaders);
PHP_METHOD(HttpRequest, getHeaders);
PHP_METHOD(HttpRequest, setHeaders);
PHP_METHOD(HttpRequest, addCookies);
PHP_METHOD(HttpRequest, getCookies);
PHP_METHOD(HttpRequest, setCookies);
PHP_METHOD(HttpRequest, enableCookies);
PHP_METHOD(HttpRequest, resetCookies);
PHP_METHOD(HttpRequest, flushCookies);
PHP_METHOD(HttpRequest, setMethod);
PHP_METHOD(HttpRequest, getMethod);
PHP_METHOD(HttpRequest, setUrl);
PHP_METHOD(HttpRequest, getUrl);
PHP_METHOD(HttpRequest, setContentType);
PHP_METHOD(HttpRequest, getContentType);
PHP_METHOD(HttpRequest, setQueryData);
PHP_METHOD(HttpRequest, getQueryData);
PHP_METHOD(HttpRequest, addQueryData);
PHP_METHOD(HttpRequest, getBody);
PHP_METHOD(HttpRequest, setBody);
PHP_METHOD(HttpRequest, addBody);
PHP_METHOD(HttpRequest, send);
PHP_METHOD(HttpRequest, getResponseData);
PHP_METHOD(HttpRequest, getResponseHeader);
PHP_METHOD(HttpRequest, getResponseCookies);
PHP_METHOD(HttpRequest, getResponseCode);
PHP_METHOD(HttpRequest, getResponseStatus);
PHP_METHOD(HttpRequest, getResponseBody);
PHP_METHOD(HttpRequest, getResponseInfo);
PHP_METHOD(HttpRequest, getResponseMessage);
PHP_METHOD(HttpRequest, getRawResponseMessage);
PHP_METHOD(HttpRequest, getRequestMessage);
PHP_METHOD(HttpRequest, getRawRequestMessage);
PHP_METHOD(HttpRequest, getHistory);
PHP_METHOD(HttpRequest, clearHistory);
PHP_METHOD(HttpRequest, getMessageClass);
PHP_METHOD(HttpRequest, setMessageClass);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

