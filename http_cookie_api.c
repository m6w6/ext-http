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

PHP_HTTP_API http_cookie_list *_http_cookie_list_init(http_cookie_list *list TSRMLS_DC)
{
	if (!list) {
		list = emalloc(sizeof(http_cookie_list));
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
	if ((SUCCESS != zend_hash_find(&list->cookies, name, name_len + 1, (void **) &cookie)) || (Z_TYPE_PP(cookie) != IS_STRING)) {
		return NULL;
	}
	return Z_STRVAL_PP(cookie);
}

PHP_HTTP_API const char *_http_cookie_list_get_extra(http_cookie_list *list, const char *name, size_t name_len TSRMLS_DC)
{
	zval **extra = NULL;
	if ((SUCCESS != zend_hash_find(&list->extras,name, name_len + 1, (void **) &extra)) || (Z_TYPE_PP(extra) != IS_STRING)) {
		return NULL;
	}
	return Z_STRVAL_PP(extra);
}

PHP_HTTP_API void _http_cookie_list_add_cookie(http_cookie_list *list, const char *name, size_t name_len, const char *value, size_t value_len TSRMLS_DC)
{
	zval *cookie_value;
	MAKE_STD_ZVAL(cookie_value);
	ZVAL_STRINGL(cookie_value, estrndup(value, value_len), value_len, 0);
	zend_hash_update(&list->cookies, name, name_len + 1, (void *) &cookie_value, sizeof(zval *), NULL);
}

PHP_HTTP_API void _http_cookie_list_add_extra(http_cookie_list *list, const char *name, size_t name_len, const char *value, size_t value_len TSRMLS_DC)
{
	zval *cookie_value;
	MAKE_STD_ZVAL(cookie_value);
	ZVAL_STRINGL(cookie_value, estrndup(value, value_len), value_len, 0);
	zend_hash_update(&list->extras, name, name_len + 1, (void *) &cookie_value, sizeof(zval *), NULL);
}

#define http_cookie_list_set_item_ex(l, i, il, v, vl, f, a) _http_cookie_list_set_item_ex((l), (i), (il), (v), (vl), (f), (a) TSRMLS_CC)
static inline void _http_cookie_list_set_item_ex(http_cookie_list *list, const char *item, int item_len, const char *value, int value_len, long flags, char **allowed_extras TSRMLS_DC)
{
	char *key = estrndup(item, item_len);
	
	if (!strcasecmp(key, "path")) {
		STR_SET(list->path, estrndup(value, value_len));
	} else if (!strcasecmp(key, "domain")) {
		STR_SET(list->domain, estrndup(value, value_len));
	} else if (!strcasecmp(key, "expires")) {
		const char *date = estrndup(value, value_len);
		list->expires = http_parse_date(date);
		efree(date);
	} else if (!strcasecmp(key, "secure")) {
		list->flags |= HTTP_COOKIE_SECURE;
	} else if (!strcasecmp(key, "httpOnly")) {
		list->flags |= HTTP_COOKIE_HTTPONLY;
	} else {
		/* check for extra */
		if (allowed_extras) {
			for (; *allowed_extras; ++allowed_extras) {
				if (!strcasecmp(key, *allowed_extras)) {
					http_cookie_list_add_extra(list, key, item_len, value, value_len);
					
					efree(key);
					return;
				}
			}
		}
		/* new cookie */
		http_cookie_list_add_cookie(list, key, item_len, value, value_len);
	}
	efree(key);
}


#define ST_QUOTE	1
#define ST_VALUE	2
#define ST_KEY		3
#define ST_ASSIGN	4
#define ST_ADD		5

/* {{{ http_cookie_list *http_parse_cookie(char *, long) */
PHP_HTTP_API http_cookie_list *_http_parse_cookie_ex(http_cookie_list *list, const char *string, long flags, char **allowed_extras TSRMLS_DC)
{
	int free_list = !list, st = ST_KEY, keylen = 0, vallen = 0;
	char *s, *c, *key = NULL, *val = NULL;
	
	list = http_cookie_list_init(list);
	
	c = s = estrdup(string);
	for(;;) {
#if 0
		char *tk = NULL, *tv = NULL;
		
		if (key) {
			if (keylen) {
				tk= estrndup(key, keylen);
			} else {
				tk = ecalloc(1, 7);
				memcpy(tk, key, 3);
				tk[3]='.'; tk[4]='.'; tk[5]='.';
			}
		}
		if (val) {
			if (vallen) {
				tv = estrndup(val, vallen);
			} else {
				tv = ecalloc(1, 7);
				memcpy(tv, val, 3);
				tv[3]='.'; tv[4]='.'; tv[5]='.';
			}
		}
		fprintf(stderr, "[%6s] %c \"%s=%s\"\n",
				(
						st == ST_QUOTE ? "QUOTE" :
						st == ST_VALUE ? "VALUE" :
						st == ST_KEY ? "KEY" :
						st == ST_ASSIGN ? "ASSIGN" :
						st == ST_ADD ? "ADD":
						"HUH?"
				), *c, tk, tv
		);
		STR_FREE(tk); STR_FREE(tv);
#endif
		switch (st)
		{
			case ST_QUOTE:
				if (*c == '"') {
					if (*(c-1) != '\\') {
						st = ST_ADD;
					} else {
						memmove(c-1, c, strlen(c)+1);
					}
				} else {
					if (!val) {
						val = c;
					}
					if (!*c) {
						--val;
						st = ST_ADD;
					}
				}
			break;
				
			case ST_VALUE:
				switch (*c)
				{
					case '"':
						if (!val) {
							st = ST_QUOTE;
						}
					break;
					
					case ' ':
					break;
					
					case ';':
						if (!*(c+1)) {
							goto add;
						} 
					case '\0':
						st = ST_ADD;
					break;
					
					default:
						if (!val) {
							val = c;
						}
					break;
				}
			break;
				
			case ST_KEY:
				switch (*c)
				{
					case ',':
					case '\r':
					case '\n':
					case '\t':
					case '\013':
					case '\014':
						goto failure;
					break;
					
					case '=':
						if (key) {
							keylen = c - key;
							st = ST_VALUE;
						} else {
							goto failure;
						}
					break;
					
					case ' ':
						if (key) {
							keylen = c - key;
							st = ST_ASSIGN;
						}
					break;
					
					case ';':
					case '\0':
						if (key) {
							keylen = c - key;
							st = ST_ADD;
						}
					break;
					
					default:
						if (!key) {
							key = c;
						}
					break;
				}
			break;
				
			case ST_ASSIGN:
				if (*c == '=') {
					st = ST_VALUE;
				} else if (!*c || *c == ';') {
					st = ST_ADD;
				} else if (*c != ' ') {
					goto failure;
				}
			break;
				
			case ST_ADD:
			add:
				if (val) {
					vallen = c - val;
					if (*c) --vallen;
					while (val[vallen-1] == ' ') --vallen;
				} else {
					val = "";
					vallen = 0;
				}
				http_cookie_list_set_item_ex(list, key, keylen, val, vallen, flags, allowed_extras);
				st = ST_KEY;
				key = val = NULL;
				keylen = vallen = 0;
			break;
		}
		
		if (*c) {
			++c;
		} else if (st == ST_ADD) {
			goto add;
		} else {
			break;
		}
	}
	
	efree(s);
	return list;
	
failure:
	http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Unexpected character (%c) at pos %tu of %zu", *c, c-s, strlen(s));
	if (free_list) {
		http_cookie_list_free(&list);
	} else {
		http_cookie_list_dtor(list);
	}
	efree(s);
	return FAILURE;
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
