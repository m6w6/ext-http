/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2013, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_init(php_http_cookie_list_t *list TSRMLS_DC)
{
	if (!list) {
		list = emalloc(sizeof(*list));
	}
	
	zend_hash_init(&list->cookies, 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_init(&list->extras, 0, NULL, ZVAL_PTR_DTOR, 0);
	
	list->path = NULL;
	list->domain = NULL;
	list->expires = -1;
	list->max_age = -1;
	list->flags = 0;
	
	TSRMLS_SET_CTX(list->ts);

	return list;
}

PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_copy(php_http_cookie_list_t *from, php_http_cookie_list_t *to)
{
	TSRMLS_FETCH_FROM_CTX(from->ts);

	to = php_http_cookie_list_init(to TSRMLS_CC);

	array_copy(&from->cookies, &to->cookies);
	array_copy(&from->extras, &to->extras);

	STR_SET(to->path, from->path ? estrdup(from->path) : NULL);
	STR_SET(to->domain, from->domain ? estrdup(from->domain) : NULL);
	to->expires = from->expires;
	to->max_age = from->max_age;
	to->flags = from->flags;

	return to;
}

PHP_HTTP_API void php_http_cookie_list_dtor(php_http_cookie_list_t *list)
{
	if (list) {
		zend_hash_destroy(&list->cookies);
		zend_hash_destroy(&list->extras);
	
		STR_SET(list->path, NULL);
		STR_SET(list->domain, NULL);
	}
}



PHP_HTTP_API void php_http_cookie_list_free(php_http_cookie_list_t **list)
{
	if (*list) {
		php_http_cookie_list_dtor(*list);
		efree(*list);
		*list = NULL;
	}
}

PHP_HTTP_API const char *php_http_cookie_list_get_cookie(php_http_cookie_list_t *list, const char *name, size_t name_len, zval **zcookie)
{
	zval **cookie;
	if ((SUCCESS != zend_symtable_find(&list->cookies, name, name_len + 1, (void *) &cookie)) || (Z_TYPE_PP(cookie) != IS_STRING)) {
		return NULL;
	}
	if (zcookie) {
		*zcookie = *cookie;
	}
	return Z_STRVAL_PP(cookie);
}

PHP_HTTP_API const char *php_http_cookie_list_get_extra(php_http_cookie_list_t *list, const char *name, size_t name_len, zval **zextra)
{
	zval **extra;

	if ((SUCCESS != zend_symtable_find(&list->extras, name, name_len + 1, (void *) &extra)) || (Z_TYPE_PP(extra) != IS_STRING)) {
		return NULL;
	}
	if (zextra) {
		*zextra = *extra;
	}
	return Z_STRVAL_PP(extra);
}

PHP_HTTP_API void php_http_cookie_list_add_cookie(php_http_cookie_list_t *list, const char *name, size_t name_len, const char *value, size_t value_len)
{
	zval *cookie_value;

	MAKE_STD_ZVAL(cookie_value);
	ZVAL_STRINGL(cookie_value, estrndup(value, value_len), value_len, 0);
	zend_symtable_update(&list->cookies, name, name_len + 1, (void *) &cookie_value, sizeof(zval *), NULL);
}

PHP_HTTP_API void php_http_cookie_list_add_extra(php_http_cookie_list_t *list, const char *name, size_t name_len, const char *value, size_t value_len)
{
	zval *cookie_value;

	MAKE_STD_ZVAL(cookie_value);
	ZVAL_STRINGL(cookie_value, estrndup(value, value_len), value_len, 0);
	zend_symtable_update(&list->extras, name, name_len + 1, (void *) &cookie_value, sizeof(zval *), NULL);
}

#define _KEY_IS(s) (key->len == sizeof(s) && !strncasecmp(key->str, (s), key->len))
static void add_entry(php_http_cookie_list_t *list, char **allowed_extras, long flags, php_http_array_hashkey_t *key, zval *val)
{
	zval *arg = php_http_zsep(1, IS_STRING, val);

	if (!(flags & PHP_HTTP_COOKIE_PARSE_RAW)) {
		Z_STRLEN_P(arg) = php_raw_url_decode(Z_STRVAL_P(arg), Z_STRLEN_P(arg));
	}

	if _KEY_IS("path") {
		STR_SET(list->path, estrndup(Z_STRVAL_P(arg), Z_STRLEN_P(arg)));
	} else if _KEY_IS("domain") {
		STR_SET(list->domain, estrndup(Z_STRVAL_P(arg), Z_STRLEN_P(arg)));
	} else if _KEY_IS("expires") {
		char *date = estrndup(Z_STRVAL_P(arg), Z_STRLEN_P(arg));
		list->expires = php_parse_date(date, NULL);
		efree(date);
	} else if _KEY_IS("max-age") {
		list->max_age = strtol(Z_STRVAL_P(arg), NULL, 10);
	} else if _KEY_IS("secure") {
		list->flags |= PHP_HTTP_COOKIE_SECURE;
	} else if _KEY_IS("httpOnly") {
		list->flags |= PHP_HTTP_COOKIE_HTTPONLY;
	} else {
		/* check for extra */
		if (allowed_extras) {
			char **ae = allowed_extras;

			php_http_array_hashkey_stringify(key);
			for (; *ae; ++ae) {
				if (!strncasecmp(key->str, *ae, key->len)) {
					if (key->type == HASH_KEY_IS_LONG) {
						zend_hash_index_update(&list->extras, key->num, (void *) &arg, sizeof(zval *), NULL);
					} else {
						zend_hash_update(&list->extras, key->str, key->len, (void *) &arg, sizeof(zval *), NULL);
					}
					php_http_array_hashkey_stringfree(key);
					return;
				}
			}
			php_http_array_hashkey_stringfree(key);
		}

		/* cookie */
		if (key->type == HASH_KEY_IS_LONG) {
			zend_hash_index_update(&list->cookies, key->num, (void *) &arg, sizeof(zval *), NULL);
		} else {
			zend_hash_update(&list->cookies, key->str, key->len, (void *) &arg, sizeof(zval *), NULL);
		}
		return;
	}
	zval_ptr_dtor(&arg);
}

PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_parse(php_http_cookie_list_t *list, const char *str, size_t len, long flags, char **allowed_extras TSRMLS_DC)
{
	php_http_params_opts_t opts;
	HashTable params;
	HashPosition pos1, pos2;
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	zval **param, **val, **args, **arg;

	php_http_params_opts_default_get(&opts);
	opts.input.str = estrndup(str, len);
	opts.input.len = len;
	opts.param = NULL;
	zend_hash_init(&params, 10, NULL, ZVAL_PTR_DTOR, 0);
	php_http_params_parse(&params, &opts TSRMLS_CC);
	efree(opts.input.str);

	list = php_http_cookie_list_init(list TSRMLS_CC);
	FOREACH_HASH_KEYVAL(pos1, &params, key, param) {
		if (Z_TYPE_PP(param) == IS_ARRAY) {
			if (SUCCESS == zend_hash_find(Z_ARRVAL_PP(param), ZEND_STRS("value"), (void *) &val)) {
				add_entry(list, NULL, flags, &key, *val);
			}
			if (SUCCESS == zend_hash_find(Z_ARRVAL_PP(param), ZEND_STRS("arguments"), (void *) &args) && Z_TYPE_PP(args) == IS_ARRAY) {
				FOREACH_KEYVAL(pos2, *args, key, arg) {
					add_entry(list, allowed_extras, flags, &key, *arg);
				}
			}
		}
	}
	zend_hash_destroy(&params);

	return list;
}

PHP_HTTP_API void php_http_cookie_list_to_struct(php_http_cookie_list_t *list, zval *strct)
{
	zval array, *cookies, *extras;
	TSRMLS_FETCH_FROM_CTX(list->ts);
	
	INIT_PZVAL_ARRAY(&array, HASH_OF(strct));
	
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
	add_assoc_long(&array, "max-age", (long) list->max_age);
	add_assoc_string(&array, "path", STR_PTR(list->path), 1);
	add_assoc_string(&array, "domain", STR_PTR(list->domain), 1);
}

PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_from_struct(php_http_cookie_list_t *list, zval *strct TSRMLS_DC)
{
	zval **tmp, *cpy;
	HashTable *ht = HASH_OF(strct);
	
	list = php_http_cookie_list_init(list TSRMLS_CC);
	
	if (SUCCESS == zend_hash_find(ht, "cookies", sizeof("cookies"), (void *) &tmp) && Z_TYPE_PP(tmp) == IS_ARRAY) {
		zend_hash_copy(&list->cookies, Z_ARRVAL_PP(tmp), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	}
	if (SUCCESS == zend_hash_find(ht, "extras", sizeof("extras"), (void *) &tmp) && Z_TYPE_PP(tmp) == IS_ARRAY) {
		zend_hash_copy(&list->extras, Z_ARRVAL_PP(tmp), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	}
	if (SUCCESS == zend_hash_find(ht, "flags", sizeof("flags"), (void *) &tmp)) {
		cpy = php_http_ztyp(IS_LONG, *tmp);
		list->flags = Z_LVAL_P(cpy);
		zval_ptr_dtor(&cpy);
	}
	if (SUCCESS == zend_hash_find(ht, "expires", sizeof("expires"), (void *) &tmp)) {
		if (Z_TYPE_PP(tmp) == IS_LONG) {
			list->expires = Z_LVAL_PP(tmp);
		} else {
			long lval;

			cpy = php_http_ztyp(IS_STRING, *tmp);
			if (IS_LONG == is_numeric_string(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy), &lval, NULL, 0)) {
				list->expires = lval;
			} else {
				list->expires = php_parse_date(Z_STRVAL_P(cpy), NULL);
			}

			zval_ptr_dtor(&cpy);
		}
	}
	if (SUCCESS == zend_hash_find(ht, "max-age", sizeof("max-age"), (void *) &tmp)) {
		if (Z_TYPE_PP(tmp) == IS_LONG) {
			list->max_age = Z_LVAL_PP(tmp);
		} else {
			long lval;

			cpy = php_http_ztyp(IS_STRING, *tmp);
			if (IS_LONG == is_numeric_string(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy), &lval, NULL, 0)) {
				list->max_age = lval;
			}

			zval_ptr_dtor(&cpy);
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

static inline void append_encoded(php_http_buffer_t *buf, const char *key, size_t key_len, const char *val, size_t val_len)
{
	char *enc_str[2];
	int enc_len[2];
	
	enc_str[0] = php_raw_url_encode(key, key_len, &enc_len[0]);
	enc_str[1] = php_raw_url_encode(val, val_len, &enc_len[1]);
	
	php_http_buffer_append(buf, enc_str[0], enc_len[0]);
	php_http_buffer_appends(buf, "=");
	php_http_buffer_append(buf, enc_str[1], enc_len[1]);
	php_http_buffer_appends(buf, "; ");
	
	efree(enc_str[0]);
	efree(enc_str[1]);
}

PHP_HTTP_API void php_http_cookie_list_to_string(php_http_cookie_list_t *list, char **str, size_t *len)
{
	php_http_buffer_t buf;
	zval **val;
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	HashPosition pos;
	TSRMLS_FETCH_FROM_CTX(list->ts);
	
	php_http_buffer_init(&buf);
	
	FOREACH_HASH_KEYVAL(pos, &list->cookies, key, val) {
		zval *tmp = php_http_ztyp(IS_STRING, *val);

		php_http_array_hashkey_stringify(&key);
		append_encoded(&buf, key.str, key.len-1, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
		php_http_array_hashkey_stringfree(&key);

		zval_ptr_dtor(&tmp);
	}
	
	if (list->domain && *list->domain) {
		php_http_buffer_appendf(&buf, "domain=%s; ", list->domain);
	}
	if (list->path && *list->path) {
		php_http_buffer_appendf(&buf, "path=%s; ", list->path);
	}
	if (list->expires >= 0) {
		char *date = php_format_date(ZEND_STRL(PHP_HTTP_DATE_FORMAT), list->expires, 0 TSRMLS_CC);
		php_http_buffer_appendf(&buf, "expires=%s; ", date);
		efree(date);
	}
	if (list->max_age >= 0) {
		php_http_buffer_appendf(&buf, "max-age=%ld; ", list->max_age);
	}
	
	FOREACH_HASH_KEYVAL(pos, &list->extras, key, val) {
		zval *tmp = php_http_ztyp(IS_STRING, *val);

		php_http_array_hashkey_stringify(&key);
		append_encoded(&buf, key.str, key.len-1, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
		php_http_array_hashkey_stringfree(&key);

		zval_ptr_dtor(&tmp);
	}
	
	if (list->flags & PHP_HTTP_COOKIE_SECURE) {
		php_http_buffer_appends(&buf, "secure; ");
	}
	if (list->flags & PHP_HTTP_COOKIE_HTTPONLY) {
		php_http_buffer_appends(&buf, "httpOnly; ");
	}
	
	php_http_buffer_fix(&buf);
	*str = buf.data;
	*len = buf.used;
}



static zend_object_handlers php_http_cookie_object_handlers;

zend_object_value php_http_cookie_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_cookie_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_cookie_object_new_ex(zend_class_entry *ce, php_http_cookie_list_t *list, php_http_cookie_object_t **ptr TSRMLS_DC)
{
	php_http_cookie_object_t *o;

	o = ecalloc(sizeof(*o), 1);
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (list) {
		o->list = list;
	}

	if (ptr) {
		*ptr = o;
	}

	o->zv.handle = zend_objects_store_put(o, NULL, php_http_cookie_object_free, NULL TSRMLS_CC);
	o->zv.handlers = &php_http_cookie_object_handlers;

	return o->zv;
}

#define PHP_HTTP_COOKIE_OBJECT_INIT(obj) \
	do { \
		if (!obj->list) { \
			obj->list = php_http_cookie_list_init(NULL TSRMLS_CC); \
		} \
	} while(0)

zend_object_value php_http_cookie_object_clone(zval *this_ptr TSRMLS_DC)
{
	php_http_cookie_object_t *new_obj, *old_obj = zend_object_store_get_object(getThis() TSRMLS_CC);
	zend_object_value ov;

	PHP_HTTP_COOKIE_OBJECT_INIT(old_obj);

	ov = php_http_cookie_object_new_ex(old_obj->zo.ce, php_http_cookie_list_copy(old_obj->list, NULL), &new_obj TSRMLS_CC);
	zend_objects_clone_members((zend_object *) new_obj, ov, (zend_object *) old_obj, Z_OBJ_HANDLE_P(getThis()) TSRMLS_CC);

	return ov;
}

void php_http_cookie_object_free(void *object TSRMLS_DC)
{
	php_http_cookie_object_t *obj = object;

	php_http_cookie_list_free(&obj->list);
	zend_object_std_dtor((zend_object *) obj TSRMLS_CC);
	efree(obj);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, cookie_string)
	ZEND_ARG_INFO(0, parser_flags)
	ZEND_ARG_INFO(0, allowed_extras)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, __construct)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		zval *zcookie = NULL;
		long flags = 0;
		HashTable *allowed_extras = NULL;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z!lH", &zcookie, &flags, &allowed_extras)) {
			if (zcookie) {
				with_error_handling(EH_THROW, php_http_exception_class_entry) {
					char **ae = NULL;


					if (allowed_extras && zend_hash_num_elements(allowed_extras)) {
						char **ae_ptr = safe_emalloc(zend_hash_num_elements(allowed_extras) + 1, sizeof(char *), 0);
						HashPosition pos;
						zval **val;

						ae = ae_ptr;
						FOREACH_HASH_VAL(pos, allowed_extras, val) {
							zval *cpy = php_http_ztyp(IS_STRING, *val);

							*ae_ptr++ = estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
							zval_ptr_dtor(&cpy);
						}
						*ae_ptr = NULL;
					}

					switch (Z_TYPE_P(zcookie)) {
						case IS_OBJECT:
							if (instanceof_function(Z_OBJCE_P(zcookie), php_http_cookie_class_entry TSRMLS_CC)) {
								php_http_cookie_object_t *zco = zend_object_store_get_object(zcookie TSRMLS_CC);

								if (zco->list) {
									obj->list = php_http_cookie_list_copy(zco->list, NULL);
								}
								break;
							}
							/* no break */
						case IS_ARRAY:
							obj->list = php_http_cookie_list_from_struct(obj->list, zcookie TSRMLS_CC);
							break;
						default: {
							zval *cpy = php_http_ztyp(IS_STRING, zcookie);

							obj->list = php_http_cookie_list_parse(obj->list, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy), flags, ae TSRMLS_CC);
							zval_ptr_dtor(&cpy);
							break;
						}
					}

					if (ae) {
						char **ae_ptr;

						for (ae_ptr = ae; *ae_ptr; ++ae_ptr) {
							efree(*ae_ptr);
						}
						efree(ae);
					}
				} end_error_handling();
			}
		}
		PHP_HTTP_COOKIE_OBJECT_INIT(obj);
	} end_error_handling();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getCookies, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getCookies)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		array_init(return_value);
		array_copy(&obj->list->cookies, Z_ARRVAL_P(return_value));
		return;
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setCookies, 0, 0, 0)
	ZEND_ARG_INFO(0, cookies)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setCookies)
{
	HashTable *cookies = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|H", &cookies)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		zend_hash_clean(&obj->list->cookies);
		if (cookies) {
			array_copy(cookies, &obj->list->cookies);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_addCookies, 0, 0, 1)
	ZEND_ARG_INFO(0, cookies)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, addCookies)
{
	HashTable *cookies = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H", &cookies)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		array_join(cookies, &obj->list->cookies, 1, ARRAY_JOIN_STRONLY);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getExtras, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, getExtras)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		array_init(return_value);
		array_copy(&obj->list->extras, Z_ARRVAL_P(return_value));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setExtras, 0, 0, 0)
	ZEND_ARG_INFO(0, extras)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setExtras)
{
	HashTable *extras = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|H", &extras)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		zend_hash_clean(&obj->list->extras);
		if (extras) {
			array_copy(extras, &obj->list->extras);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_addExtras, 0, 0, 1)
	ZEND_ARG_INFO(0, extras)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, addExtras)
{
	HashTable *extras = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H", &extras)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		array_join(extras, &obj->list->extras, 1, ARRAY_JOIN_STRONLY);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getCookie, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, getCookie)
{
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name_str, &name_len)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		zval *zvalue;

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		if (php_http_cookie_list_get_cookie(obj->list, name_str, name_len, &zvalue)) {
			RETURN_ZVAL(zvalue, 1, 0);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setCookie, 0, 0, 1)
	ZEND_ARG_INFO(0, cookie_name)
	ZEND_ARG_INFO(0, cookie_value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setCookie)
{
	char *name_str, *value_str = NULL;
	int name_len, value_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s!", &name_str, &name_len, &value_str, &value_len)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		if (!value_str) {
			php_http_cookie_list_del_cookie(obj->list, name_str, name_len);
		} else {
			php_http_cookie_list_add_cookie(obj->list, name_str, name_len, value_str, value_len);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_addCookie, 0, 0, 2)
	ZEND_ARG_INFO(0, cookie_name)
	ZEND_ARG_INFO(0, cookie_value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, addCookie)
{
	char *name_str, *value_str;
	int name_len, value_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &name_str, &name_len, &value_str, &value_len)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		php_http_cookie_list_add_cookie(obj->list, name_str, name_len, value_str, value_len);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getExtra, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, getExtra)
{
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name_str, &name_len)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		zval *zvalue;

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		if (php_http_cookie_list_get_extra(obj->list, name_str, name_len, &zvalue)) {
			RETURN_ZVAL(zvalue, 1, 0);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setExtra, 0, 0, 1)
	ZEND_ARG_INFO(0, extra_name)
	ZEND_ARG_INFO(0, extra_value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setExtra)
{
	char *name_str, *value_str = NULL;
	int name_len, value_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s!", &name_str, &name_len, &value_str, &value_len)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		if (!value_str) {
			php_http_cookie_list_del_extra(obj->list, name_str, name_len);
		} else {
			php_http_cookie_list_add_extra(obj->list, name_str, name_len, value_str, value_len);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_addExtra, 0, 0, 2)
	ZEND_ARG_INFO(0, extra_name)
	ZEND_ARG_INFO(0, extra_value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, addExtra)
{
	char *name_str, *value_str;
	int name_len, value_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &name_str, &name_len, &value_str, &value_len)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		php_http_cookie_list_add_extra(obj->list, name_str, name_len, value_str, value_len);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getDomain, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getDomain)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		if (obj->list->domain) {
			RETURN_STRING(obj->list->domain, 1);
		}
		RETURN_NULL();
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setDomain, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setDomain)
{
	char *domain_str = NULL;
	int domain_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!", &domain_str, &domain_len)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		STR_SET(obj->list->domain, domain_str ? estrndup(domain_str, domain_len) : NULL);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getPath, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getPath)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		if (obj->list->path) {
			RETURN_STRING(obj->list->path, 1);
		}
		RETURN_NULL();
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setPath, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setPath)
{
	char *path_str = NULL;
	int path_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!", &path_str, &path_len)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		STR_SET(obj->list->path, path_str ? estrndup(path_str, path_len) : NULL);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getExpires, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getExpires)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		RETURN_LONG(obj->list->expires);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setExpires, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setExpires)
{
	long ts = -1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &ts)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		obj->list->expires = ts;
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getMaxAge, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getMaxAge)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		RETURN_LONG(obj->list->max_age);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setMaxAge, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setMaxAge)
{
	long ts = -1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &ts)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		obj->list->max_age = ts;
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getFlags, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, getFlags)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		RETURN_LONG(obj->list->flags);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setFlags, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setFlags)
{
	long flags = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &flags)) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		obj->list->flags = flags;
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_toString, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, toString)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *str;
		size_t len;

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		php_http_cookie_list_to_string(obj->list, &str, &len);
		RETURN_STRINGL(str, len, 0);
	}
	RETURN_EMPTY_STRING();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_toArray, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, toArray)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_cookie_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_COOKIE_OBJECT_INIT(obj);

		array_init(return_value);
		php_http_cookie_list_to_struct(obj->list, return_value);
	}
}

static zend_function_entry php_http_cookie_methods[] = {
	PHP_ME(HttpCookie, __construct,   ai_HttpCookie___construct,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, getCookies,    ai_HttpCookie_getCookies,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, setCookies,    ai_HttpCookie_setCookies,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, addCookies,    ai_HttpCookie_addCookies,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, getCookie,     ai_HttpCookie_getCookie,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, setCookie,     ai_HttpCookie_setCookie,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, addCookie,     ai_HttpCookie_addCookie,    ZEND_ACC_PUBLIC)

	PHP_ME(HttpCookie, getExtras,     ai_HttpCookie_getExtras,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, setExtras,     ai_HttpCookie_setExtras,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, addExtras,     ai_HttpCookie_addExtras,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, getExtra,      ai_HttpCookie_getExtra,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, setExtra,      ai_HttpCookie_setExtra,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, addExtra,      ai_HttpCookie_addExtra,     ZEND_ACC_PUBLIC)

	PHP_ME(HttpCookie, getDomain,     ai_HttpCookie_getDomain,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, setDomain,     ai_HttpCookie_setDomain,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, getPath,       ai_HttpCookie_getPath,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, setPath,       ai_HttpCookie_setPath,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, getExpires,    ai_HttpCookie_getExpires,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, setExpires,    ai_HttpCookie_setExpires,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, getMaxAge,     ai_HttpCookie_getMaxAge,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, setMaxAge,     ai_HttpCookie_setMaxAge,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, getFlags,      ai_HttpCookie_getFlags,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, setFlags,      ai_HttpCookie_setFlags,     ZEND_ACC_PUBLIC)

	PHP_ME(HttpCookie, toArray,       ai_HttpCookie_toArray,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpCookie, toString,      ai_HttpCookie_toString,     ZEND_ACC_PUBLIC)
	ZEND_MALIAS(HttpCookie, __toString, toString, ai_HttpCookie_toString, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};

zend_class_entry *php_http_cookie_class_entry;

PHP_MINIT_FUNCTION(http_cookie)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Cookie", php_http_cookie_methods);
	php_http_cookie_class_entry = zend_register_internal_class_ex(&ce, php_http_object_class_entry, NULL TSRMLS_CC);
	php_http_cookie_class_entry->create_object = php_http_cookie_object_new;
	memcpy(&php_http_cookie_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_cookie_object_handlers.clone_obj = php_http_cookie_object_clone;

	zend_declare_class_constant_long(php_http_cookie_class_entry, ZEND_STRL("PARSE_RAW"), PHP_HTTP_COOKIE_PARSE_RAW TSRMLS_CC);
	zend_declare_class_constant_long(php_http_cookie_class_entry, ZEND_STRL("SECURE"), PHP_HTTP_COOKIE_SECURE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_cookie_class_entry, ZEND_STRL("HTTPONLY"), PHP_HTTP_COOKIE_HTTPONLY TSRMLS_CC);

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
