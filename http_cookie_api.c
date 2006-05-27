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

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_date_api.h"
#include "php_http_cookie_api.h"

PHP_MINIT_FUNCTION(http_cookie)
{
	HTTP_LONG_CONSTANT("HTTP_COOKIE_PARSE_RAW", HTTP_COOKIE_PARSE_RAW);
	HTTP_LONG_CONSTANT("HTTP_COOKIE_SECURE", HTTP_COOKIE_SECURE);
	HTTP_LONG_CONSTANT("HTTP_COOKIE_HTTPONLY", HTTP_COOKIE_HTTPONLY);
	
	return SUCCESS;
}

PHP_HTTP_API http_cookie_list *_http_cookie_list_init(http_cookie_list *list ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	if (!list) {
		list = emalloc_rel(sizeof(http_cookie_list));
	}
	
	zend_hash_init(&list->cookies, 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_init(&list->extras, 0, NULL, ZVAL_PTR_DTOR, 0);
	
	list->path = NULL;
	list->domain = NULL;
	list->expires = 0;
	list->flags = 0;
	
	return list;
}

PHP_HTTP_API void _http_cookie_list_dtor(http_cookie_list *list TSRMLS_DC)
{
	if (list) {
		zend_hash_destroy(&list->cookies);
		zend_hash_destroy(&list->extras);
	
		STR_SET(list->path, NULL);
		STR_SET(list->domain, NULL);
	}
}

PHP_HTTP_API void _http_cookie_list_free(http_cookie_list **list TSRMLS_DC)
{
	if (list) {
		http_cookie_list_dtor(*list);
		efree(*list);
		*list = NULL;
	}
}

PHP_HTTP_API const char *_http_cookie_list_get_cookie(http_cookie_list *list, const char *name, size_t name_len TSRMLS_DC)
{
	zval **cookie = NULL;
	if ((SUCCESS != zend_hash_find(&list->cookies, (char *) name, name_len + 1, (void *) &cookie)) || (Z_TYPE_PP(cookie) != IS_STRING)) {
		return NULL;
	}
	return Z_STRVAL_PP(cookie);
}

PHP_HTTP_API const char *_http_cookie_list_get_extra(http_cookie_list *list, const char *name, size_t name_len TSRMLS_DC)
{
	zval **extra = NULL;
	if ((SUCCESS != zend_hash_find(&list->extras, (char *) name, name_len + 1, (void *) &extra)) || (Z_TYPE_PP(extra) != IS_STRING)) {
		return NULL;
	}
	return Z_STRVAL_PP(extra);
}

PHP_HTTP_API void _http_cookie_list_add_cookie(http_cookie_list *list, const char *name, size_t name_len, const char *value, size_t value_len TSRMLS_DC)
{
	zval *cookie_value;
	char *key = estrndup(name, name_len);
	MAKE_STD_ZVAL(cookie_value);
	ZVAL_STRINGL(cookie_value, estrndup(value, value_len), value_len, 0);
	zend_hash_update(&list->cookies, key, name_len + 1, (void *) &cookie_value, sizeof(zval *), NULL);
	efree(key);
}

PHP_HTTP_API void _http_cookie_list_add_extra(http_cookie_list *list, const char *name, size_t name_len, const char *value, size_t value_len TSRMLS_DC)
{
	zval *cookie_value;
	char *key = estrndup(name, name_len);
	MAKE_STD_ZVAL(cookie_value);
	ZVAL_STRINGL(cookie_value, estrndup(value, value_len), value_len, 0);
	zend_hash_update(&list->extras, key, name_len + 1, (void *) &cookie_value, sizeof(zval *), NULL);
	efree(key);
}

typedef struct _http_parse_param_cb_arg_t {
	http_cookie_list *list;
	long flags;
	char **allowed_extras;
} http_parse_param_cb_arg;

static void http_parse_cookie_callback(void *ptr, const char *key, int keylen, const char *val, int vallen TSRMLS_DC)
{
	http_parse_param_cb_arg *arg = (http_parse_param_cb_arg *) ptr;
	
#define _KEY_IS(s) (keylen == lenof(s) && !strncasecmp(key, (s), keylen))
	if _KEY_IS("path") {
		STR_SET(arg->list->path, estrndup(val, vallen));
	} else if _KEY_IS("domain") {
		STR_SET(arg->list->domain, estrndup(val, vallen));
	} else if _KEY_IS("expires") {
		char *date = estrndup(val, vallen);
		arg->list->expires = http_parse_date(date);
		efree(date);
	} else if _KEY_IS("secure") {
		arg->list->flags |= HTTP_COOKIE_SECURE;
	} else if _KEY_IS("httpOnly") {
		arg->list->flags |= HTTP_COOKIE_HTTPONLY;
	} else {
		/* check for extra */
		if (arg->allowed_extras) {
			char **ae = arg->allowed_extras;
			
			for (; *ae; ++ae) {
				if ((size_t) keylen == strlen(*ae) && !strncasecmp(key, *ae, keylen)) {
					http_cookie_list_add_extra(arg->list, key, keylen, val, vallen);
					return;
				}
			}
		}
		/* new cookie */
		http_cookie_list_add_cookie(arg->list, key, keylen, val, vallen);
	}
}

/* {{{ http_cookie_list *http_parse_cookie(char *, long) */
PHP_HTTP_API http_cookie_list *_http_parse_cookie_ex(http_cookie_list *list, const char *string, long flags, char **allowed_extras TSRMLS_DC)
{
	int free_list = !list;
	http_parse_param_cb_arg arg;
	
	list = http_cookie_list_init(list);
	
	arg.list = list;
	arg.flags = flags;
	arg.allowed_extras = allowed_extras;
	
	if (SUCCESS != http_parse_params_ex(string, HTTP_PARAMS_RAISE_ERROR, http_parse_cookie_callback, &arg)) {
		if (free_list) {
			http_cookie_list_free(&list);
		} else {
			http_cookie_list_dtor(list);
		}
		list = NULL;
	}
	
	return list;
}
/* }}} */

PHP_HTTP_API void _http_cookie_list_tostruct(http_cookie_list *list, zval *strct TSRMLS_DC)
{
	zval array, *cookies, *extras;
	
	INIT_ZARR(array, HASH_OF(strct));
	
	MAKE_STD_ZVAL(cookies);
	array_init(cookies);
	zend_hash_copy(Z_ARRVAL_P(cookies), &list->cookies, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	add_assoc_zval(&array, "cookies", cookies);
	
	MAKE_STD_ZVAL(extras);
	array_init(extras);
	zend_hash_copy(Z_ARRVAL_P(extras), &list->extras, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	add_assoc_zval(&array, "extras", extras);
	
	add_assoc_long(&array, "flags", list->flags);
	add_assoc_long(&array, "expires", (long) list->expires);
	add_assoc_string(&array, "path", list->path?list->path:"", 1);
	add_assoc_string(&array, "domain", list->domain?list->domain:"", 1);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
