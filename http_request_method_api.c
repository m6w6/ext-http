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

#include "php_http_api.h"
#include "php_http_request_api.h"
#include "php_http_request_method_api.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL) && !defined(WONKY)
#	include "php_http_request_object.h"
#endif

/* {{{ char *http_request_methods[] */
static const char *const http_request_methods[] = {
	"UNKNOWN",
	/* HTTP/1.1 */
	"GET",
	"HEAD",
	"POST",
	"PUT",
	"DELETE",
	"OPTIONS",
	"TRACE",
	"CONNECT",
	/* WebDAV - RFC 2518 */
	"PROPFIND",
	"PROPPATCH",
	"MKCOL",
	"COPY",
	"MOVE",
	"LOCK",
	"UNLOCK",
	/* WebDAV Versioning - RFC 3253 */
	"VERSION-CONTROL",
	"REPORT",
	"CHECKOUT",
	"CHECKIN",
	"UNCHECKOUT",
	"MKWORKSPACE",
	"UPDATE",
	"LABEL",
	"MERGE",
	"BASELINE-CONTROL",
	"MKACTIVITY",
	/* WebDAV Access Control - RFC 3744 */
	"ACL",
	NULL
};
/* }}} */

/* {{{ */
PHP_MINIT_FUNCTION(http_request_method)
{
	/* HTTP/1.1 */
	HTTP_LONG_CONSTANT("HTTP_METH_GET", HTTP_GET);
	HTTP_LONG_CONSTANT("HTTP_METH_HEAD", HTTP_HEAD);
	HTTP_LONG_CONSTANT("HTTP_METH_POST", HTTP_POST);
	HTTP_LONG_CONSTANT("HTTP_METH_PUT", HTTP_PUT);
	HTTP_LONG_CONSTANT("HTTP_METH_DELETE", HTTP_DELETE);
	HTTP_LONG_CONSTANT("HTTP_METH_OPTIONS", HTTP_OPTIONS);
	HTTP_LONG_CONSTANT("HTTP_METH_TRACE", HTTP_TRACE);
	HTTP_LONG_CONSTANT("HTTP_METH_CONNECT", HTTP_CONNECT);
	/* WebDAV - RFC 2518 */
	HTTP_LONG_CONSTANT("HTTP_METH_PROPFIND", HTTP_PROPFIND);
	HTTP_LONG_CONSTANT("HTTP_METH_PROPPATCH", HTTP_PROPPATCH);
	HTTP_LONG_CONSTANT("HTTP_METH_MKCOL", HTTP_MKCOL);
	HTTP_LONG_CONSTANT("HTTP_METH_COPY", HTTP_COPY);
	HTTP_LONG_CONSTANT("HTTP_METH_MOVE", HTTP_MOVE);
	HTTP_LONG_CONSTANT("HTTP_METH_LOCK", HTTP_LOCK);
	HTTP_LONG_CONSTANT("HTTP_METH_UNLOCK", HTTP_UNLOCK);
	/* WebDAV Versioning - RFC 3253 */
	HTTP_LONG_CONSTANT("HTTP_METH_VERSION_CONTROL", HTTP_VERSION_CONTROL);
	HTTP_LONG_CONSTANT("HTTP_METH_REPORT", HTTP_REPORT);
	HTTP_LONG_CONSTANT("HTTP_METH_CHECKOUT", HTTP_CHECKOUT);
	HTTP_LONG_CONSTANT("HTTP_METH_CHECKIN", HTTP_CHECKIN);
	HTTP_LONG_CONSTANT("HTTP_METH_UNCHECKOUT", HTTP_UNCHECKOUT);
	HTTP_LONG_CONSTANT("HTTP_METH_MKWORKSPACE", HTTP_MKWORKSPACE);
	HTTP_LONG_CONSTANT("HTTP_METH_UPDATE", HTTP_UPDATE);
	HTTP_LONG_CONSTANT("HTTP_METH_LABEL", HTTP_LABEL);
	HTTP_LONG_CONSTANT("HTTP_METH_MERGE", HTTP_MERGE);
	HTTP_LONG_CONSTANT("HTTP_METH_BASELINE_CONTROL", HTTP_BASELINE_CONTROL);
	HTTP_LONG_CONSTANT("HTTP_METH_MKACTIVITY", HTTP_MKACTIVITY);
	/* WebDAV Access Control - RFC 3744 */
	HTTP_LONG_CONSTANT("HTTP_METH_ACL", HTTP_ACL);
	
	return SUCCESS;
}

PHP_RINIT_FUNCTION(http_request_method)
{
	HTTP_G->request.methods.custom.entries = ecalloc(1, sizeof(http_request_method_entry *));
	
	if (HTTP_G->request.methods.custom.ini && *HTTP_G->request.methods.custom.ini) {
		HashPosition pos;
		HashTable methods;
		zval **data;
	
		zend_hash_init(&methods, 0, NULL, ZVAL_PTR_DTOR, 0);
		http_parse_params(HTTP_G->request.methods.custom.ini, HTTP_PARAMS_DEFAULT, &methods);
		FOREACH_HASH_VAL(pos, &methods, data) {
			if (Z_TYPE_PP(data) == IS_STRING) {
				http_request_method_register(Z_STRVAL_PP(data), Z_STRLEN_PP(data));
			}
		}
		zend_hash_destroy(&methods);
	}
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http_request_method)
{
	int i;
	http_request_method_entry **ptr = HTTP_G->request.methods.custom.entries;
	
	for (i = 0; i < HTTP_G->request.methods.custom.count; ++i) {
		if (ptr[i]) {
			http_request_method_unregister(HTTP_CUSTOM_REQUEST_METHOD_START + i);
		}
	}
	efree(HTTP_G->request.methods.custom.entries);
	
	return SUCCESS;
}
/* }}} */

/* {{{ char *http_request_method_name(http_request_method) */
PHP_HTTP_API const char *_http_request_method_name(http_request_method m TSRMLS_DC)
{
	http_request_method_entry **ptr = HTTP_G->request.methods.custom.entries;

	if (HTTP_STD_REQUEST_METHOD(m)) {
		return http_request_methods[m];
	}

	if (	(HTTP_CUSTOM_REQUEST_METHOD(m) >= 0) && 
			(HTTP_CUSTOM_REQUEST_METHOD(m) < HTTP_G->request.methods.custom.count) &&
			(ptr[HTTP_CUSTOM_REQUEST_METHOD(m)])) {
		return ptr[HTTP_CUSTOM_REQUEST_METHOD(m)]->name;
	}

	return http_request_methods[0];
}
/* }}} */

/* {{{ int http_request_method_exists(zend_bool, ulong, char *) */
PHP_HTTP_API int _http_request_method_exists(zend_bool by_name, http_request_method id, const char *name TSRMLS_DC)
{
	int i;
	http_request_method_entry **ptr = HTTP_G->request.methods.custom.entries;
	
	if (by_name) {
		for (i = HTTP_MIN_REQUEST_METHOD; i < HTTP_MAX_REQUEST_METHOD; ++i) {
			if (!strcasecmp(name, http_request_methods[i])) {
				return i;
			}
		}
		for (i = 0; i < HTTP_G->request.methods.custom.count; ++i) {
			if (ptr[i] && !strcasecmp(name, ptr[i]->name)) {
				return HTTP_CUSTOM_REQUEST_METHOD_START + i;
			}
		}
	} else if (HTTP_STD_REQUEST_METHOD(id)) {
		return id;
	} else if (	(HTTP_CUSTOM_REQUEST_METHOD(id) >= 0) && 
				(HTTP_CUSTOM_REQUEST_METHOD(id) < HTTP_G->request.methods.custom.count) && 
				(ptr[HTTP_CUSTOM_REQUEST_METHOD(id)])) {
		return id;
	}
	
	return 0;
}
/* }}} */

/* {{{ int http_request_method_register(char *) */
PHP_HTTP_API int _http_request_method_register(const char *method_name, int method_name_len TSRMLS_DC)
{
	int i, meth_num;
	char *http_method, *method, *mconst;
	http_request_method_entry **ptr = HTTP_G->request.methods.custom.entries;
	
	if (!HTTP_IS_CTYPE(alpha, *method_name)) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Request method does not start with a character (%s)", method_name);
		return 0;
	}
	
	if (http_request_method_exists(1, 0, method_name)) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Request method does already exist (%s)", method_name);
		return 0;
	}
	
	method = emalloc(method_name_len + 1);
	mconst = emalloc(method_name_len + 1);
	for (i = 0; i < method_name_len; ++i) {
		switch (method_name[i]) {
			case '-':
				method[i] = '-';
				mconst[i] = '_';
				break;
			
			default:
				if (!HTTP_IS_CTYPE(alnum, method_name[i])) {
					efree(method);
					efree(mconst);
					http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Request method contains illegal characters (%s)", method_name);
					return 0;
				}
				mconst[i] = method[i] = HTTP_TO_CTYPE(upper, method_name[i]);
				break;
		}
	}
	method[method_name_len] = '\0';
	mconst[method_name_len] = '\0';
	
	ptr = erealloc(ptr, sizeof(http_request_method_entry *) * (HTTP_G->request.methods.custom.count + 1));
	HTTP_G->request.methods.custom.entries = ptr;
	ptr[HTTP_G->request.methods.custom.count] = emalloc(sizeof(http_request_method_entry));
	ptr[HTTP_G->request.methods.custom.count]->name = method;
	ptr[HTTP_G->request.methods.custom.count]->cnst = mconst;
	meth_num = HTTP_CUSTOM_REQUEST_METHOD_START + HTTP_G->request.methods.custom.count++;

	method_name_len = spprintf(&http_method, 0, "HTTP_METH_%s", mconst);
	zend_register_long_constant(http_method, method_name_len + 1, meth_num, CONST_CS, http_module_number TSRMLS_CC);
	efree(http_method);
	
#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL) && !defined(WONKY)
	method_name_len = spprintf(&http_method, 0, "METH_%s", mconst);
	zend_declare_class_constant_long(http_request_object_ce, http_method, method_name_len, meth_num TSRMLS_CC);
	efree(http_method);
#endif
	
	return meth_num;
}
/* }}} */

/* {{{ STATUS http_request_method_unregister(int) */
PHP_HTTP_API STATUS _http_request_method_unregister(int method TSRMLS_DC)
{
	char *http_method;
	int method_len;
	http_request_method_entry **ptr = HTTP_G->request.methods.custom.entries;
	
	if (HTTP_STD_REQUEST_METHOD(method)) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Standard request methods cannot be unregistered");
		return FAILURE;
	}

	if (	(HTTP_CUSTOM_REQUEST_METHOD(method) < 0) ||
			(HTTP_CUSTOM_REQUEST_METHOD(method) > HTTP_G->request.methods.custom.count) ||
			(!ptr[HTTP_CUSTOM_REQUEST_METHOD(method)])) {
		http_error_ex(HE_NOTICE, HTTP_E_REQUEST_METHOD, "Custom request method with id %d does not exist", method);
		return FAILURE;
	}
	
#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL) && !defined(WONKY)
	method_len = spprintf(&http_method, 0, "METH_%s", ptr[HTTP_CUSTOM_REQUEST_METHOD(method)]->cnst);
	if (SUCCESS != zend_hash_del(&http_request_object_ce->constants_table, http_method, method_len + 1)) {
		http_error_ex(HE_NOTICE, HTTP_E_REQUEST_METHOD, "Could not unregister request method: HttpRequest::%s", http_method);
		efree(http_method);
		return FAILURE;
	}
	efree(http_method);
#endif
	
	method_len = spprintf(&http_method, 0, "HTTP_METH_%s", ptr[HTTP_CUSTOM_REQUEST_METHOD(method)]->cnst);
	if (SUCCESS != zend_hash_del(EG(zend_constants), http_method, method_len + 1)) {
		http_error_ex(HE_NOTICE, HTTP_E_REQUEST_METHOD, "Could not unregister request method: %s", http_method);
		efree(http_method);
		return FAILURE;
	}
	efree(http_method);
	
	efree(ptr[HTTP_CUSTOM_REQUEST_METHOD(method)]->name);
	efree(ptr[HTTP_CUSTOM_REQUEST_METHOD(method)]->cnst);
	efree(ptr[HTTP_CUSTOM_REQUEST_METHOD(method)]);
	ptr[HTTP_CUSTOM_REQUEST_METHOD(method)] = NULL;
	
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

