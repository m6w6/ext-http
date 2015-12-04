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

#ifndef PHP_HTTP_COOKIE_H
#define PHP_HTTP_COOKIE_H

#define PHP_HTTP_COOKIE_SECURE		0x10L
#define PHP_HTTP_COOKIE_HTTPONLY	0x20L

#define PHP_HTTP_COOKIE_PARSE_RAW	0x01L

/*
	generally a netscape cookie compliant struct, recognizing httpOnly attribute, too;
	cookie params like those from rfc2109 and rfc2965 are just put into extras, if
	one specifies them in allowed extras, else they're treated like cookies themself
*/
typedef struct php_http_cookie_list {
	HashTable cookies;
	HashTable extras;
	long flags;
	char *path;
	char *domain;
	time_t expires;
	time_t max_age;
} php_http_cookie_list_t;

PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_init(php_http_cookie_list_t *list);
PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_parse(php_http_cookie_list_t *list, const char *str, size_t len, long flags, char **allowed_extras);
PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_copy(php_http_cookie_list_t *from, php_http_cookie_list_t *to);
PHP_HTTP_API void php_http_cookie_list_dtor(php_http_cookie_list_t *list);
PHP_HTTP_API void php_http_cookie_list_free(php_http_cookie_list_t **list);

#define php_http_cookie_list_has_cookie(list, name, name_len) zend_symtable_str_exists(&(list)->cookies, (name), (name_len))
#define php_http_cookie_list_del_cookie(list, name, name_len) zend_symtable_str_del(&(list)->cookies, (name), (name_len))
PHP_HTTP_API void php_http_cookie_list_add_cookie(php_http_cookie_list_t *list, const char *name, size_t name_len, const char *value, size_t value_len);
PHP_HTTP_API const char *php_http_cookie_list_get_cookie(php_http_cookie_list_t *list, const char *name, size_t name_len, zval *cookie);

#define php_http_cookie_list_has_extra(list, name, name_len) zend_symtable_str_exists(&(list)->extras, (name), (name_len))
#define php_http_cookie_list_del_extra(list, name, name_len) zend_symtable_str_del(&(list)->extras, (name), (name_len))
PHP_HTTP_API void php_http_cookie_list_add_extra(php_http_cookie_list_t *list, const char *name, size_t name_len, const char *value, size_t value_len);
PHP_HTTP_API const char *php_http_cookie_list_get_extra(php_http_cookie_list_t *list, const char *name, size_t name_len, zval *extra);

PHP_HTTP_API void php_http_cookie_list_to_string(php_http_cookie_list_t *list, char **str, size_t *len);
PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_from_struct(php_http_cookie_list_t *list, zval *strct);
PHP_HTTP_API void php_http_cookie_list_to_struct(php_http_cookie_list_t *list, zval *strct);

PHP_HTTP_API zend_class_entry *php_http_cookie_get_class_entry(void);

typedef struct php_http_cookie_object {
	php_http_cookie_list_t *list;
	zend_object zo;
} php_http_cookie_object_t;

zend_object *php_http_cookie_object_new(zend_class_entry *ce);
php_http_cookie_object_t *php_http_cookie_object_new_ex(zend_class_entry *ce, php_http_cookie_list_t *list);
zend_object *php_http_cookie_object_clone(zval *this_ptr);
void php_http_cookie_object_free(zend_object *object);

PHP_MINIT_FUNCTION(http_cookie);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
