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

static void free_method(void *el)
{
	efree(*(char **)el);
}

static void unregister_method(const char *name TSRMLS_DC)
{
	char *ptr, tmp[sizeof("HTTP_METH_") + HTTP_REQUEST_METHOD_MAXLEN] = "HTTP_METH_";
	
	strlcpy(tmp + lenof("HTTP_METH_"), name, HTTP_REQUEST_METHOD_MAXLEN);
	for (ptr = tmp + lenof("HTTP_METH_"); *ptr; ++ptr) {
		if (*ptr == '-') {
			*ptr = '_';
		}
	}
	
#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL) && !defined(WONKY)
	if (SUCCESS != zend_hash_del(&http_request_object_ce->constants_table, tmp + lenof("HTTP_"), strlen(tmp + lenof("HTTP_")) + 1)) {
		http_error_ex(HE_NOTICE, HTTP_E_REQUEST_METHOD, "Could not unregister request method: HttpRequest::%s", tmp + lenof("HTTP_"));
	}
#endif
	if (SUCCESS != zend_hash_del(EG(zend_constants), tmp, strlen(tmp) + 1)) {
		http_error_ex(HE_NOTICE, HTTP_E_REQUEST_METHOD, "Could not unregister request method: %s", tmp);
	}
}

PHP_RINIT_FUNCTION(http_request_method)
{
	HashTable ht;
	
	zend_hash_init(&HTTP_G->request.methods.registered, 0, NULL, free_method, 0);
#define HTTP_METH_REG(m) \
	{ \
		char *_m=estrdup(m); \
		zend_hash_next_index_insert(&HTTP_G->request.methods.registered, (void *) &_m, sizeof(char *), NULL); \
	}
	HTTP_METH_REG("UNKNOWN");
	/* HTTP/1.1 */
	HTTP_METH_REG("GET");
	HTTP_METH_REG("HEAD");
	HTTP_METH_REG("POST");
	HTTP_METH_REG("PUT");
	HTTP_METH_REG("DELETE");
	HTTP_METH_REG("OPTIONS");
	HTTP_METH_REG("TRACE");
	HTTP_METH_REG("CONNECT");
	/* WebDAV - RFC 2518 */
	HTTP_METH_REG("PROPFIND");
	HTTP_METH_REG("PROPPATCH");
	HTTP_METH_REG("MKCOL");
	HTTP_METH_REG("COPY");
	HTTP_METH_REG("MOVE");
	HTTP_METH_REG("LOCK");
	HTTP_METH_REG("UNLOCK");
	/* WebDAV Versioning - RFC 3253 */
	HTTP_METH_REG("VERSION-CONTROL");
	HTTP_METH_REG("REPORT");
	HTTP_METH_REG("CHECKOUT");
	HTTP_METH_REG("CHECKIN");
	HTTP_METH_REG("UNCHECKOUT");
	HTTP_METH_REG("MKWORKSPACE");
	HTTP_METH_REG("UPDATE");
	HTTP_METH_REG("LABEL");
	HTTP_METH_REG("MERGE");
	HTTP_METH_REG("BASELINE-CONTROL");
	HTTP_METH_REG("MKACTIVITY");
	/* WebDAV Access Control - RFC 3744 */
	HTTP_METH_REG("ACL");
	
	zend_hash_init(&ht, 0, NULL, ZVAL_PTR_DTOR, 0);
	if (*HTTP_G->request.methods.custom && SUCCESS == http_parse_params(HTTP_G->request.methods.custom, HTTP_PARAMS_DEFAULT, &ht)) {
		HashPosition pos;
		zval **val;
		
		FOREACH_HASH_VAL(pos, &ht, val) {
			if (Z_TYPE_PP(val) == IS_STRING) {
				http_request_method_register(Z_STRVAL_PP(val), Z_STRLEN_PP(val));
			}
		}
	}
	zend_hash_destroy(&ht);
	
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http_request_method)
{
	char **name;
	int i, c = zend_hash_next_free_element(&HTTP_G->request.methods.registered);
	
	for (i = HTTP_MAX_REQUEST_METHOD; i < c; ++i) {
		if (SUCCESS == zend_hash_index_find(&HTTP_G->request.methods.registered, i, (void *) &name)) {
			unregister_method(*name TSRMLS_CC);
		}
	}
	
	zend_hash_destroy(&HTTP_G->request.methods.registered);
	return SUCCESS;
}

#define http_request_method_cncl(m, c) _http_request_method_cncl_ex((m), strlen(m), (c) TSRMLS_CC)
#define http_request_method_cncl_ex(m, l, c) _http_request_method_cncl_ex((m), (l), (c) TSRMLS_CC)
static STATUS _http_request_method_cncl_ex(const char *method_name, int method_name_len, char **cnst TSRMLS_DC)
{
	int i;
	char *cncl;
	
	if (method_name_len >= HTTP_REQUEST_METHOD_MAXLEN) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Request method too long (%s)", method_name);
	}
	cncl = emalloc(method_name_len + 1);
	
	for (i = 0; i < method_name_len; ++i) {
		switch (method_name[i]) {
			case '-':
				cncl[i] = '-';
				break;
			
			default:
				if (!HTTP_IS_CTYPE(alnum, method_name[i])) {
					efree(cncl);
					http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Request method contains illegal characters (%s)", method_name);
					return FAILURE;
				}
				cncl[i] = HTTP_TO_CTYPE(upper, method_name[i]);
				break;
		}
	}
	cncl[method_name_len] = '\0';
	
	*cnst = cncl;
	return SUCCESS;
}

PHP_HTTP_API const char *_http_request_method_name(http_request_method m TSRMLS_DC)
{
	char **name;
	
	if (SUCCESS == zend_hash_index_find(&HTTP_G->request.methods.registered, m, (void *) &name)) {
		return *name;
	}
	return "UNKNOWN";
}

PHP_HTTP_API int _http_request_method_exists(int by_name, http_request_method id_num, const char *id_str TSRMLS_DC)
{
	char *id_dup;
	
	if (by_name && (SUCCESS == http_request_method_cncl(id_str, &id_dup))) {
		char **name;
		HashPosition pos;
		HashKey key = initHashKey(0);
		
		FOREACH_HASH_KEYVAL(pos, &HTTP_G->request.methods.registered, key, name) {
			if (key.type == HASH_KEY_IS_LONG && !strcmp(*name, id_dup)) {
				efree(id_dup);
				return key.num;
			}
		}
		efree(id_dup);
	} else if (zend_hash_index_exists(&HTTP_G->request.methods.registered, id_num)){
		return id_num;
	}
	return 0;
}

PHP_HTTP_API int _http_request_method_register(const char *method_str, int method_len TSRMLS_DC)
{
	char *method_dup, *ptr, tmp[sizeof("HTTP_METH_") + HTTP_REQUEST_METHOD_MAXLEN] = "HTTP_METH_";
	int method_num = http_request_method_exists(1, 0, method_str);
	
	if (!method_num && (SUCCESS == http_request_method_cncl_ex(method_str, method_len, &method_dup))) {
		method_num = zend_hash_next_free_element(&HTTP_G->request.methods.registered);
		zend_hash_index_update(&HTTP_G->request.methods.registered, method_num, (void *) &method_dup, sizeof(char *), NULL);
		
		strlcpy(tmp + lenof("HTTP_METH_"), method_dup, HTTP_REQUEST_METHOD_MAXLEN);
		for (ptr = tmp + lenof("HTTP_METH_"); *ptr; ++ptr) {
			if (*ptr == '-') {
				*ptr = '_';
			}
		}
		
		zend_register_long_constant(tmp, strlen(tmp) + 1, method_num, CONST_CS, http_module_number TSRMLS_CC);
#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL) && !defined(WONKY)
		zend_declare_class_constant_long(http_request_object_ce, tmp + lenof("HTTP_"), strlen(tmp + lenof("HTTP_")), method_num TSRMLS_CC);
#endif
	}
	
	return method_num;
}

PHP_HTTP_API STATUS _http_request_method_unregister(int method TSRMLS_DC)
{
	char **name;
	
	if (HTTP_STD_REQUEST_METHOD(method)) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Standard request methods cannot be unregistered");
		return FAILURE;
	}

	if (SUCCESS != zend_hash_index_find(&HTTP_G->request.methods.registered, method, (void *) &name)) {
		http_error_ex(HE_NOTICE, HTTP_E_REQUEST_METHOD, "Custom request method with id %d does not exist", method);
		return FAILURE;
	}
	
	unregister_method(*name TSRMLS_CC);
	
	zend_hash_index_del(&HTTP_G->request.methods.registered, method);
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

