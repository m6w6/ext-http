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

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_date_api.h"
#include "php_http_cookie_api.h"

#include "ext/standard/url.h"

/* {{{ PHP_MINIT_FUNCTION(http_cookie) */
PHP_MINIT_FUNCTION(http_cookie)
{
	HTTP_LONG_CONSTANT("HTTP_COOKIE_PARSE_RAW", HTTP_COOKIE_PARSE_RAW);
	HTTP_LONG_CONSTANT("HTTP_COOKIE_SECURE", HTTP_COOKIE_SECURE);
	HTTP_LONG_CONSTANT("HTTP_COOKIE_HTTPONLY", HTTP_COOKIE_HTTPONLY);
	
	return SUCCESS;
}
/* }}} */

/* {{{ http_cookie_list *http_cookie_list_init(http_cookie_list *) */
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
/* }}} */

/* {{{ void http_cookie_list_dtor(http_cookie_list *) */
PHP_HTTP_API void _http_cookie_list_dtor(http_cookie_list *list TSRMLS_DC)
{
	if (list) {
		zend_hash_destroy(&list->cookies);
		zend_hash_destroy(&list->extras);
	
		STR_SET(list->path, NULL);
		STR_SET(list->domain, NULL);
	}
}
/* }}} */

/* {{{ void http_cookie_list_free(http_cookie_list **) */
PHP_HTTP_API void _http_cookie_list_free(http_cookie_list **list TSRMLS_DC)
{
	if (list) {
		http_cookie_list_dtor(*list);
		efree(*list);
		*list = NULL;
	}
}
/* }}} */

/* {{{ const char *http_cookie_list_get_cookie(http_cookie_list *, const char*, size_t) */
PHP_HTTP_API const char *_http_cookie_list_get_cookie(http_cookie_list *list, const char *name, size_t name_len TSRMLS_DC)
{
	zval **cookie = NULL;
	if ((SUCCESS != zend_hash_find(&list->cookies, (char *) name, name_len + 1, (void *) &cookie)) || (Z_TYPE_PP(cookie) != IS_STRING)) {
		return NULL;
	}
	return Z_STRVAL_PP(cookie);
}
/* }}} */

/* {{{ const char *http_cookie_list_get_extra(http_cookie_list *, const char *, size_t) */
PHP_HTTP_API const char *_http_cookie_list_get_extra(http_cookie_list *list, const char *name, size_t name_len TSRMLS_DC)
{
	zval **extra = NULL;
	if ((SUCCESS != zend_hash_find(&list->extras, (char *) name, name_len + 1, (void *) &extra)) || (Z_TYPE_PP(extra) != IS_STRING)) {
		return NULL;
	}
	return Z_STRVAL_PP(extra);
}
/* }}} */

/* {{{ void http_cookie_list_add_cookie(http_cookie_list *, const char *, size_t, const char *, size_t) */
PHP_HTTP_API void _http_cookie_list_add_cookie(http_cookie_list *list, const char *name, size_t name_len, const char *value, size_t value_len TSRMLS_DC)
{
	zval *cookie_value;
	char *key = estrndup(name, name_len);
	MAKE_STD_ZVAL(cookie_value);
	ZVAL_STRINGL(cookie_value, estrndup(value, value_len), value_len, 0);
	zend_hash_update(&list->cookies, key, name_len + 1, (void *) &cookie_value, sizeof(zval *), NULL);
	efree(key);
}
/* }}} */

/* {{{ void http_cookie_list_add_extr(http_cookie_list *, const char *, size_t, const char *, size_t) */
PHP_HTTP_API void _http_cookie_list_add_extra(http_cookie_list *list, const char *name, size_t name_len, const char *value, size_t value_len TSRMLS_DC)
{
	zval *cookie_value;
	char *key = estrndup(name, name_len);
	MAKE_STD_ZVAL(cookie_value);
	ZVAL_STRINGL(cookie_value, estrndup(value, value_len), value_len, 0);
	zend_hash_update(&list->extras, key, name_len + 1, (void *) &cookie_value, sizeof(zval *), NULL);
	efree(key);
}
/* }}} */

typedef struct _http_parse_param_cb_arg_t {
	http_cookie_list *list;
	long flags;
	char **allowed_extras;
} http_parse_param_cb_arg;

/* {{{ static void http_parse_cookie_callback */
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
					if (arg->flags & HTTP_COOKIE_PARSE_RAW) {
						http_cookie_list_add_extra(arg->list, key, keylen, val, vallen);
					} else {
						char *dec = estrndup(val, vallen);
						int declen = php_url_decode(dec, vallen);
						
						http_cookie_list_add_extra(arg->list, key, keylen, dec, declen);
						efree(dec);
					}
					return;
				}
			}
		}
		/* new cookie */
		if (arg->flags & HTTP_COOKIE_PARSE_RAW) {
			http_cookie_list_add_cookie(arg->list, key, keylen, val, vallen);
		} else {
			char *dec = estrndup(val, vallen);
			int declen = php_url_decode(dec, vallen);
			
			http_cookie_list_add_cookie(arg->list, key, keylen, dec, declen);
			efree(dec);
		}
	}
}
/* }}} */

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

/* {{{ void http_cookie_list_tostruct(http_cookie_list *, zval *) */
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
	add_assoc_string(&array, "path", STR_PTR(list->path), 1);
	add_assoc_string(&array, "domain", STR_PTR(list->domain), 1);
}
/* }}} */

/* {{{ http_cookie_list *http_cookie_list_fromstruct(http_cookie_list *, zval *strct) */
PHP_HTTP_API http_cookie_list *_http_cookie_list_fromstruct(http_cookie_list *list, zval *strct TSRMLS_DC)
{
	zval **tmp, *cpy;
	HashTable *ht = HASH_OF(strct);
	
	list = http_cookie_list_init(list);
	
	if (SUCCESS == zend_hash_find(ht, "cookies", sizeof("cookies"), (void *) &tmp) && Z_TYPE_PP(tmp) == IS_ARRAY) {
		zend_hash_copy(&list->cookies, Z_ARRVAL_PP(tmp), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	}
	if (SUCCESS == zend_hash_find(ht, "extras", sizeof("extras"), (void *) &tmp) && Z_TYPE_PP(tmp) == IS_ARRAY) {
		zend_hash_copy(&list->extras, Z_ARRVAL_PP(tmp), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	}
	if (SUCCESS == zend_hash_find(ht, "flags", sizeof("flags"), (void *) &tmp)) {
		switch (Z_TYPE_PP(tmp)) {
			case IS_LONG:
				list->flags = Z_LVAL_PP(tmp);
				break;
			case IS_DOUBLE:
				list->flags = (long) Z_DVAL_PP(tmp);
				break;
			case IS_STRING:
				cpy = zval_copy(IS_LONG, *tmp);
				list->flags = Z_LVAL_P(cpy);
				zval_free(&cpy);
				break;
			default:
				break;
		}
	}
	if (SUCCESS == zend_hash_find(ht, "expires", sizeof("expires"), (void *) &tmp)) {
		switch (Z_TYPE_PP(tmp)) {
			case IS_LONG:
				list->expires = Z_LVAL_PP(tmp);
				break;
			case IS_DOUBLE:
				list->expires = (long) Z_DVAL_PP(tmp);
				break;
			case IS_STRING:
				cpy = zval_copy(IS_LONG, *tmp);
				if (Z_LVAL_P(cpy)) {
					list->expires = Z_LVAL_P(cpy);
				} else {
					time_t expires = http_parse_date(Z_STRVAL_PP(tmp));
					if (expires > 0) {
						list->expires = expires;
					}
				}
				zval_free(&cpy);
				break;
			default:
				break;
		}
	}
	if (SUCCESS == zend_hash_find(ht, "path", sizeof("path"), (void *) &tmp) && Z_TYPE_PP(tmp) == IS_STRING) {
		list->path = estrndup(Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp));
	}
	if (SUCCESS == zend_hash_find(ht, "domain", sizeof("domain"), (void *) &tmp) && Z_TYPE_PP(tmp) == IS_STRING) {
		list->domain = estrndup(Z_STRVAL_PP(tmp), Z_STRLEN_PP(tmp));
	}
	
	return list;
}
/* }}} */

/* {{{ inline append_encoded */
static inline void append_encoded(phpstr *buf, const char *key, size_t key_len, const char *val, size_t val_len)
{
	char *enc_str[2];
	int enc_len[2];
	
	enc_str[0] = php_url_encode(key, key_len, &enc_len[0]);
	enc_str[1] = php_url_encode(val, val_len, &enc_len[1]);
	
	phpstr_append(buf, enc_str[0], enc_len[0]);
	phpstr_appends(buf, "=");
	phpstr_append(buf, enc_str[1], enc_len[1]);
	phpstr_appends(buf, "; ");
	
	efree(enc_str[0]);
	efree(enc_str[1]);
}
/* }}} */

/* {{{ void http_cookie_list_tostring(http_cookie_list *, char **, size_t *) */
PHP_HTTP_API void _http_cookie_list_tostring(http_cookie_list *list, char **str, size_t *len TSRMLS_DC)
{
	phpstr buf;
	zval **val;
	HashKey key = initHashKey(0);
	HashPosition pos;
	
	phpstr_init(&buf);
	
	FOREACH_HASH_KEYVAL(pos, &list->cookies, key, val) {
		if (key.type == HASH_KEY_IS_STRING && key.len) {
			append_encoded(&buf, key.str, key.len-1, Z_STRVAL_PP(val), Z_STRLEN_PP(val));
		}
	}
	
	if (list->domain && *list->domain) {
		phpstr_appendf(&buf, "domain=%s; ", list->domain);
	}
	if (list->path && *list->path) {
		phpstr_appendf(&buf, "path=%s; ", list->path);
	}
	if (list->expires) {
		char *date = http_date(list->expires);
		phpstr_appendf(&buf, "expires=%s; ", date);
		efree(date);
	}
	
	FOREACH_HASH_KEYVAL(pos, &list->extras, key, val) {
		if (key.type == HASH_KEY_IS_STRING && key.len) {
			append_encoded(&buf, key.str, key.len-1, Z_STRVAL_PP(val), Z_STRLEN_PP(val));
		}
	}
	
	if (list->flags & HTTP_COOKIE_SECURE) {
		phpstr_appends(&buf, "secure; ");
	}
	if (list->flags & HTTP_COOKIE_HTTPONLY) {
		phpstr_appends(&buf, "httpOnly; ");
	}
	
	phpstr_fix(&buf);
	*str = PHPSTR_VAL(&buf);
	*len = PHPSTR_LEN(&buf);
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
