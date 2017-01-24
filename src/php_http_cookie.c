/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

php_http_cookie_list_t *php_http_cookie_list_init(php_http_cookie_list_t *list)
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
	
	return list;
}

php_http_cookie_list_t *php_http_cookie_list_copy(php_http_cookie_list_t *from, php_http_cookie_list_t *to)
{
	to = php_http_cookie_list_init(to);

	array_copy(&from->cookies, &to->cookies);
	array_copy(&from->extras, &to->extras);

	PTR_SET(to->path, from->path ? estrdup(from->path) : NULL);
	PTR_SET(to->domain, from->domain ? estrdup(from->domain) : NULL);
	to->expires = from->expires;
	to->max_age = from->max_age;
	to->flags = from->flags;

	return to;
}

void php_http_cookie_list_dtor(php_http_cookie_list_t *list)
{
	if (list) {
		zend_hash_destroy(&list->cookies);
		zend_hash_destroy(&list->extras);
	
		PTR_SET(list->path, NULL);
		PTR_SET(list->domain, NULL);
	}
}



void php_http_cookie_list_free(php_http_cookie_list_t **list)
{
	if (*list) {
		php_http_cookie_list_dtor(*list);
		efree(*list);
		*list = NULL;
	}
}

const char *php_http_cookie_list_get_cookie(php_http_cookie_list_t *list, const char *name, size_t name_len, zval *zcookie)
{
	zval *cookie = zend_symtable_str_find(&list->cookies, name, name_len);

	if (!cookie || (Z_TYPE_P(cookie) != IS_STRING)) {
		return NULL;
	}
	if (zcookie) {
		*zcookie = *cookie;
	}
	return Z_STRVAL_P(cookie);
}

const char *php_http_cookie_list_get_extra(php_http_cookie_list_t *list, const char *name, size_t name_len, zval *zextra)
{
	zval *extra = zend_symtable_str_find(&list->extras, name, name_len);

	if (!extra || (Z_TYPE_P(extra) != IS_STRING)) {
		return NULL;
	}
	if (zextra) {
		*zextra = *extra;
	}
	return Z_STRVAL_P(extra);
}

void php_http_cookie_list_add_cookie(php_http_cookie_list_t *list, const char *name, size_t name_len, const char *value, size_t value_len)
{
	zval cookie_value;

	ZVAL_STRINGL(&cookie_value, value, value_len);
	zend_symtable_str_update(&list->cookies, name, name_len, &cookie_value);
}

void php_http_cookie_list_add_extra(php_http_cookie_list_t *list, const char *name, size_t name_len, const char *value, size_t value_len)
{
	zval extra_value;

	ZVAL_STRINGL(&extra_value, value, value_len);
	zend_symtable_str_update(&list->extras, name, name_len, &extra_value);
}

#define _KEY_IS(s) (key->key && key->key->len == sizeof(s)-1 && !strncasecmp(key->key->val, (s), key->key->len))
static void add_entry(php_http_cookie_list_t *list, char **allowed_extras, long flags, zend_hash_key *key, zval *val)
{
	zval arg;

	ZVAL_DUP(&arg, val);
	convert_to_string(&arg);

	if (!(flags & PHP_HTTP_COOKIE_PARSE_RAW)) {
		Z_STRLEN(arg) = php_raw_url_decode(Z_STRVAL(arg), Z_STRLEN(arg));
		zend_string_forget_hash_val(Z_STR(arg));
	}

	if _KEY_IS("path") {
		PTR_SET(list->path, estrndup(Z_STRVAL(arg), Z_STRLEN(arg)));
	} else if _KEY_IS("domain") {
		PTR_SET(list->domain, estrndup(Z_STRVAL(arg), Z_STRLEN(arg)));
	} else if _KEY_IS("expires") {
		char *date = estrndup(Z_STRVAL(arg), Z_STRLEN(arg));
		list->expires = php_parse_date(date, NULL);
		efree(date);
	} else if _KEY_IS("max-age") {
		list->max_age = zval_get_long(val);
	} else if _KEY_IS("secure") {
		list->flags |= PHP_HTTP_COOKIE_SECURE;
	} else if _KEY_IS("httpOnly") {
		list->flags |= PHP_HTTP_COOKIE_HTTPONLY;
	} else {
		php_http_arrkey_t tmp = {0};

		php_http_arrkey_stringify(&tmp, key);

		/* check for extra */
		if (allowed_extras) {
			char **ae = allowed_extras;
			for (; *ae; ++ae) {
				if (!strncasecmp(*ae, tmp.key->val, tmp.key->len)) {
					zend_symtable_update(&list->extras, tmp.key, &arg);
					php_http_arrkey_dtor(&tmp);
					return;
				}
			}
		}

		/* cookie */
		zend_symtable_update(&list->cookies, tmp.key, &arg);

		php_http_arrkey_dtor(&tmp);
		return;
	}

	zval_ptr_dtor(&arg);
}

php_http_cookie_list_t *php_http_cookie_list_parse(php_http_cookie_list_t *list, const char *str, size_t len, long flags, char **allowed_extras)
{
	php_http_params_opts_t opts;
	HashTable params;
	zend_hash_key k, arg_k;
	zval *param, *val, *args, *arg;

	php_http_params_opts_default_get(&opts);
	opts.input.str = estrndup(str, len);
	opts.input.len = len;
	opts.param = NULL;
	zend_hash_init(&params, 10, NULL, ZVAL_PTR_DTOR, 0);
	php_http_params_parse(&params, &opts);
	efree(opts.input.str);

	list = php_http_cookie_list_init(list);
	ZEND_HASH_FOREACH_KEY_VAL(&params, k.h, k.key, param)
	{
		if (Z_TYPE_P(param) == IS_ARRAY) {
			if ((val = zend_hash_str_find(Z_ARRVAL_P(param), ZEND_STRL("value")))) {
				add_entry(list, NULL, flags, &k, val);
			}
			if ((args = zend_hash_str_find(Z_ARRVAL_P(param), ZEND_STRL("arguments"))) && Z_TYPE_P(args) == IS_ARRAY) {
				ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(args), arg_k.h, arg_k.key, arg)
				{
					add_entry(list, allowed_extras, flags, &arg_k, arg);
				}
				ZEND_HASH_FOREACH_END();
			}
		}
	}
	ZEND_HASH_FOREACH_END();

	zend_hash_destroy(&params);

	return list;
}

void php_http_cookie_list_to_struct(php_http_cookie_list_t *list, zval *strct)
{
	zval cookies, extras, tmp;
	HashTable *ht = HASH_OF(strct);
	
	array_init_size(&cookies, zend_hash_num_elements(&list->cookies));
	array_copy(&list->cookies, Z_ARRVAL(cookies));
	zend_symtable_str_update(ht, ZEND_STRL("cookies"), &cookies);
	
	array_init_size(&extras, zend_hash_num_elements(&list->extras));
	array_copy(&list->extras, Z_ARRVAL(extras));
	zend_symtable_str_update(ht, ZEND_STRL("extras"), &extras);
	
	ZVAL_LONG(&tmp, list->flags);
	zend_symtable_str_update(ht, ZEND_STRL("flags"), &tmp);
	ZVAL_LONG(&tmp, list->expires);
	zend_symtable_str_update(ht, ZEND_STRL("expires"), &tmp);
	ZVAL_LONG(&tmp, list->max_age);
	zend_symtable_str_update(ht, ZEND_STRL("max-age"), &tmp);
	ZVAL_STRING(&tmp, STR_PTR(list->path));
	zend_symtable_str_update(ht, ZEND_STRL("path"), &tmp);
	ZVAL_STRING(&tmp, STR_PTR(list->domain));
	zend_symtable_str_update(ht, ZEND_STRL("domain"), &tmp);
}

php_http_cookie_list_t *php_http_cookie_list_from_struct(php_http_cookie_list_t *list, zval *strct)
{
	zval *tmp;
	HashTable *ht;

	ht = HASH_OF(strct);
	list = php_http_cookie_list_init(list);

	if ((tmp = zend_hash_str_find_ind(ht, ZEND_STRL("cookies"))) && Z_TYPE_P(tmp) == IS_ARRAY){
		array_copy(Z_ARRVAL_P(tmp), &list->cookies);
	}
	if ((tmp = zend_hash_str_find_ind(ht, ZEND_STRL("extras"))) && Z_TYPE_P(tmp) == IS_ARRAY){
		array_copy(Z_ARRVAL_P(tmp), &list->extras);
	}
	if ((tmp = zend_hash_str_find_ind(ht, ZEND_STRL("flags")))) {
		list->flags = zval_get_long(tmp);
	}
	if ((tmp = zend_hash_str_find_ind(ht, ZEND_STRL("expires")))) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			list->expires = Z_LVAL_P(tmp);
		} else {
			zend_long lval;
			zend_string *lstr = zval_get_string(tmp);

			if (IS_LONG == is_numeric_string(lstr->val, lstr->len, &lval, NULL, 0)) {
				list->expires = lval;
			} else {
				list->expires = php_parse_date(lstr->val, NULL);
			}

			zend_string_release(lstr);
		}
	}
	if ((tmp = zend_hash_str_find_ind(ht, ZEND_STRL("max-age")))) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			list->max_age = Z_LVAL_P(tmp);
		} else {
			zend_long lval;
			zend_string *lstr = zval_get_string(tmp);

			if (IS_LONG == is_numeric_string(lstr->val, lstr->len, &lval, NULL, 0)) {
				list->max_age = lval;
			}

			zend_string_release(lstr);
		}
	}
	if ((tmp = zend_hash_str_find_ind(ht, ZEND_STRL("path")))) {
		zend_string *str = zval_get_string(tmp);

		list->path = estrndup(str->val, str->len);
		zend_string_release(str);
	}
	if ((tmp = zend_hash_str_find_ind(ht, ZEND_STRL("domain")))) {
		zend_string *str = zval_get_string(tmp);

		list->domain = estrndup(str->val, str->len);
		zend_string_release(str);
	}
	
	return list;
}

static inline void append_encoded(php_http_buffer_t *buf, const char *key, size_t key_len, const char *val, size_t val_len)
{
	zend_string *enc_str[2];
	
	enc_str[0] = php_raw_url_encode(key, key_len);
	enc_str[1] = php_raw_url_encode(val, val_len);
	
	php_http_buffer_append(buf, enc_str[0]->val, enc_str[0]->len);
	php_http_buffer_appends(buf, "=");
	php_http_buffer_append(buf, enc_str[1]->val, enc_str[1]->len);
	php_http_buffer_appends(buf, "; ");
	
	zend_string_release(enc_str[0]);
	zend_string_release(enc_str[1]);
}

void php_http_cookie_list_to_string(php_http_cookie_list_t *list, char **str, size_t *len)
{
	php_http_buffer_t buf;
	zend_hash_key key;
	zval *val;
	
	php_http_buffer_init(&buf);

	ZEND_HASH_FOREACH_KEY_VAL(&list->cookies, key.h, key.key, val)
	{
		zend_string *str = zval_get_string(val);
		php_http_arrkey_t arrkey = {0};

		php_http_arrkey_stringify(&arrkey, &key);
		append_encoded(&buf, arrkey.key->val, arrkey.key->len, str->val, str->len);
		php_http_arrkey_dtor(&arrkey);
		zend_string_release(str);
	}
	ZEND_HASH_FOREACH_END();
	
	if (list->domain && *list->domain) {
		php_http_buffer_appendf(&buf, "domain=%s; ", list->domain);
	}
	if (list->path && *list->path) {
		php_http_buffer_appendf(&buf, "path=%s; ", list->path);
	}
	if (list->expires >= 0) {
		zend_string *date = php_format_date(ZEND_STRL(PHP_HTTP_DATE_FORMAT), list->expires, 0);
		php_http_buffer_appendf(&buf, "expires=%s; ", date->val);
		zend_string_release(date);
	}
	if (list->max_age >= 0) {
		php_http_buffer_appendf(&buf, "max-age=%ld; ", list->max_age);
	}
	
	ZEND_HASH_FOREACH_KEY_VAL(&list->extras, key.h, key.key, val)
	{
		zend_string *str = zval_get_string(val);
		php_http_arrkey_t arrkey;

		php_http_arrkey_stringify(&arrkey, &key);
		append_encoded(&buf, arrkey.key->val, arrkey.key->len, str->val, str->len);
		php_http_arrkey_dtor(&arrkey);
		zend_string_release(str);
	}
	ZEND_HASH_FOREACH_END();
	
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


static zend_class_entry *php_http_cookie_class_entry;
zend_class_entry *php_http_cookie_get_class_entry(void)
{
	return php_http_cookie_class_entry;
}

static zend_object_handlers php_http_cookie_object_handlers;

zend_object *php_http_cookie_object_new(zend_class_entry *ce)
{
	return &php_http_cookie_object_new_ex(ce, NULL)->zo;
}

php_http_cookie_object_t *php_http_cookie_object_new_ex(zend_class_entry *ce, php_http_cookie_list_t *list)
{
	php_http_cookie_object_t *o;

	if (!ce) {
		ce = php_http_cookie_class_entry;
	}

	o = ecalloc(1, sizeof(*o) + zend_object_properties_size(ce));
	zend_object_std_init(&o->zo, ce);
	object_properties_init(&o->zo, ce);
	o->zo.handlers = &php_http_cookie_object_handlers;

	if (list) {
		o->list = list;
	}

	return o;
}

#define PHP_HTTP_COOKIE_OBJECT_INIT(obj) \
	do { \
		if (!obj->list) { \
			obj->list = php_http_cookie_list_init(NULL); \
		} \
	} while(0)

zend_object *php_http_cookie_object_clone(zval *obj)
{
	php_http_cookie_object_t *new_obj, *old_obj = PHP_HTTP_OBJ(NULL, obj);

	PHP_HTTP_COOKIE_OBJECT_INIT(old_obj);

	new_obj = php_http_cookie_object_new_ex(old_obj->zo.ce, php_http_cookie_list_copy(old_obj->list, NULL));
	zend_objects_clone_members(&new_obj->zo, &old_obj->zo);

	return &new_obj->zo;
}

void php_http_cookie_object_free(zend_object *object)
{
	php_http_cookie_object_t *obj = PHP_HTTP_OBJ(object, NULL);

	php_http_cookie_list_free(&obj->list);
	zend_object_std_dtor(object);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, cookie_string)
	ZEND_ARG_INFO(0, parser_flags)
	ZEND_ARG_INFO(0, allowed_extras)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, __construct)
{
	php_http_cookie_object_t *obj;
	zval *zcookie = NULL;
	zend_long flags = 0;
	char **ae = NULL;
	HashTable *allowed_extras = NULL;
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|z!lH/", &zcookie, &flags, &allowed_extras), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	zend_replace_error_handling(EH_THROW, php_http_get_exception_runtime_class_entry(), &zeh);
	if (zcookie) {

		if (allowed_extras && zend_hash_num_elements(allowed_extras)) {
			char **ae_ptr = safe_emalloc(zend_hash_num_elements(allowed_extras) + 1, sizeof(char *), 0);
			zval *val;

			ae = ae_ptr;
			ZEND_HASH_FOREACH_VAL(allowed_extras, val)
			{
				zend_string *str = zval_get_string(val);

				*ae_ptr++ = estrndup(str->val, str->len);
				zend_string_release(str);
			}
			ZEND_HASH_FOREACH_END();
			*ae_ptr = NULL;
		}

		switch (Z_TYPE_P(zcookie)) {
			case IS_OBJECT:
				if (instanceof_function(Z_OBJCE_P(zcookie), php_http_cookie_class_entry)) {
					php_http_cookie_object_t *zco = PHP_HTTP_OBJ(NULL, zcookie);

					if (zco->list) {
						obj->list = php_http_cookie_list_copy(zco->list, NULL);
					}
					break;
				}
				/* no break */
			case IS_ARRAY:
				obj->list = php_http_cookie_list_from_struct(obj->list, zcookie);
				break;
			default: {
				zend_string *str = zval_get_string(zcookie);

				obj->list = php_http_cookie_list_parse(obj->list, str->val, str->len, flags, ae);
				zend_string_release(str);
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
	}
	zend_restore_error_handling(&zeh);

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getCookies, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getCookies)
{
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	array_init_size(return_value, zend_hash_num_elements(&obj->list->cookies));
	array_copy(&obj->list->cookies, Z_ARRVAL_P(return_value));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setCookies, 0, 0, 0)
	ZEND_ARG_INFO(0, cookies)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setCookies)
{
	HashTable *cookies = NULL;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|H/", &cookies), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	zend_hash_clean(&obj->list->cookies);
	if (cookies) {
		array_copy_strings(cookies, &obj->list->cookies);
	}

	RETURN_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_addCookies, 0, 0, 1)
	ZEND_ARG_INFO(0, cookies)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, addCookies)
{
	HashTable *cookies = NULL;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "H/", &cookies), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	array_join(cookies, &obj->list->cookies, 1, ARRAY_JOIN_STRONLY|ARRAY_JOIN_STRINGIFY);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getExtras, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, getExtras)
{
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	array_init_size(return_value, zend_hash_num_elements(&obj->list->extras));
	array_copy(&obj->list->extras, Z_ARRVAL_P(return_value));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setExtras, 0, 0, 0)
	ZEND_ARG_INFO(0, extras)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setExtras)
{
	HashTable *extras = NULL;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|H/", &extras), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	zend_hash_clean(&obj->list->extras);
	if (extras) {
		array_copy_strings(extras, &obj->list->extras);
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_addExtras, 0, 0, 1)
	ZEND_ARG_INFO(0, extras)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, addExtras)
{
	HashTable *extras = NULL;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "H/", &extras), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	array_join(extras, &obj->list->extras, 1, ARRAY_JOIN_STRONLY|ARRAY_JOIN_STRINGIFY);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getCookie, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, getCookie)
{
	char *name_str;
	size_t name_len;
	zval zvalue;
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name_str, &name_len)) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	if (php_http_cookie_list_get_cookie(obj->list, name_str, name_len, &zvalue)) {
		RETURN_ZVAL(&zvalue, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setCookie, 0, 0, 1)
	ZEND_ARG_INFO(0, cookie_name)
	ZEND_ARG_INFO(0, cookie_value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setCookie)
{
	char *name_str, *value_str = NULL;
	size_t name_len, value_len = 0;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|s!", &name_str, &name_len, &value_str, &value_len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	if (!value_str) {
		php_http_cookie_list_del_cookie(obj->list, name_str, name_len);
	} else {
		php_http_cookie_list_add_cookie(obj->list, name_str, name_len, value_str, value_len);
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
	size_t name_len, value_len;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &name_str, &name_len, &value_str, &value_len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	php_http_cookie_list_add_cookie(obj->list, name_str, name_len, value_str, value_len);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getExtra, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, getExtra)
{
	char *name_str;
	size_t name_len;
	zval zvalue;
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "s", &name_str, &name_len)) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	if (php_http_cookie_list_get_extra(obj->list, name_str, name_len, &zvalue)) {
		RETURN_ZVAL(&zvalue, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setExtra, 0, 0, 1)
	ZEND_ARG_INFO(0, extra_name)
	ZEND_ARG_INFO(0, extra_value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setExtra)
{
	char *name_str, *value_str = NULL;
	size_t name_len, value_len = 0;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|s!", &name_str, &name_len, &value_str, &value_len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	if (!value_str) {
		php_http_cookie_list_del_extra(obj->list, name_str, name_len);
	} else {
		php_http_cookie_list_add_extra(obj->list, name_str, name_len, value_str, value_len);
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
	size_t name_len, value_len;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &name_str, &name_len, &value_str, &value_len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	php_http_cookie_list_add_extra(obj->list, name_str, name_len, value_str, value_len);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getDomain, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getDomain)
{
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	if (obj->list->domain) {
		RETURN_STRING(obj->list->domain);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setDomain, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setDomain)
{
	char *domain_str = NULL;
	size_t domain_len = 0;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|s!", &domain_str, &domain_len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	PTR_SET(obj->list->domain, domain_str ? estrndup(domain_str, domain_len) : NULL);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getPath, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getPath)
{
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	if (obj->list->path) {
		RETURN_STRING(obj->list->path);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setPath, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setPath)
{
	char *path_str = NULL;
	size_t path_len = 0;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|s!", &path_str, &path_len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	PTR_SET(obj->list->path, path_str ? estrndup(path_str, path_len) : NULL);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getExpires, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getExpires)
{
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	RETURN_LONG(obj->list->expires);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setExpires, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setExpires)
{
	zend_long ts = -1;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &ts), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	obj->list->expires = ts;

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getMaxAge, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, getMaxAge)
{
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	RETURN_LONG(obj->list->max_age);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setMaxAge, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setMaxAge)
{
	zend_long ma = -1;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &ma), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	obj->list->max_age = ma;

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_getFlags, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, getFlags)
{
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	RETURN_LONG(obj->list->flags);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_setFlags, 0, 0, 0)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpCookie, setFlags)
{
	zend_long flags = 0;
	php_http_cookie_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &flags), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	obj->list->flags = flags;

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_toString, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, toString)
{
	php_http_cookie_object_t *obj;
	char *str;
	size_t len;

	if (SUCCESS != zend_parse_parameters_none()) {
		RETURN_EMPTY_STRING();
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	php_http_cookie_list_to_string(obj->list, &str, &len);

	RETURN_NEW_STR(php_http_cs2zs(str, len));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpCookie_toArray, 0, 0, 0)
ZEND_END_ARG_INFO();;
static PHP_METHOD(HttpCookie, toArray)
{
	php_http_cookie_object_t *obj;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_COOKIE_OBJECT_INIT(obj);

	array_init_size(return_value, 8);
	php_http_cookie_list_to_struct(obj->list, return_value);
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

PHP_MINIT_FUNCTION(http_cookie)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Cookie", php_http_cookie_methods);
	php_http_cookie_class_entry = zend_register_internal_class(&ce);
	php_http_cookie_class_entry->create_object = php_http_cookie_object_new;
	memcpy(&php_http_cookie_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_cookie_object_handlers.offset = XtOffsetOf(php_http_cookie_object_t, zo);
	php_http_cookie_object_handlers.clone_obj = php_http_cookie_object_clone;
	php_http_cookie_object_handlers.free_obj = php_http_cookie_object_free;

	zend_declare_class_constant_long(php_http_cookie_class_entry, ZEND_STRL("PARSE_RAW"), PHP_HTTP_COOKIE_PARSE_RAW);
	zend_declare_class_constant_long(php_http_cookie_class_entry, ZEND_STRL("SECURE"), PHP_HTTP_COOKIE_SECURE);
	zend_declare_class_constant_long(php_http_cookie_class_entry, ZEND_STRL("HTTPONLY"), PHP_HTTP_COOKIE_HTTPONLY);

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
