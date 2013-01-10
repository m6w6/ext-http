/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2011, Michael Wallner <mike@php.net>            |
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

#ifdef ZTS
	void ***ts;
#endif
} php_http_cookie_list_t;

PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_init(php_http_cookie_list_t *list TSRMLS_DC);
PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_parse(php_http_cookie_list_t *list, const char *str, size_t len, long flags, char **allowed_extras TSRMLS_DC);
PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_copy(php_http_cookie_list_t *from, php_http_cookie_list_t *to);
PHP_HTTP_API void php_http_cookie_list_dtor(php_http_cookie_list_t *list);
PHP_HTTP_API void php_http_cookie_list_free(php_http_cookie_list_t **list);

#define php_http_cookie_list_has_cookie(list, name, name_len) zend_symtable_exists(&(list)->cookies, (name), (name_len)+1)
#define php_http_cookie_list_del_cookie(list, name, name_len) zend_symtable_del(&(list)->cookies, (name), (name_len)+1)
PHP_HTTP_API void php_http_cookie_list_add_cookie(php_http_cookie_list_t *list, const char *name, size_t name_len, const char *value, size_t value_len);
PHP_HTTP_API const char *php_http_cookie_list_get_cookie(php_http_cookie_list_t *list, const char *name, size_t name_len, zval **cookie);

#define php_http_cookie_list_has_extra(list, name, name_len) zend_symtable_exists(&(list)->extras, (name), (name_len)+1)
#define php_http_cookie_list_del_extra(list, name, name_len) zend_symtable_del(&(list)->extras, (name), (name_len)+1)
PHP_HTTP_API void php_http_cookie_list_add_extra(php_http_cookie_list_t *list, const char *name, size_t name_len, const char *value, size_t value_len);
PHP_HTTP_API const char *php_http_cookie_list_get_extra(php_http_cookie_list_t *list, const char *name, size_t name_len, zval **extra);

PHP_HTTP_API void php_http_cookie_list_to_string(php_http_cookie_list_t *list, char **str, size_t *len);
PHP_HTTP_API php_http_cookie_list_t *php_http_cookie_list_from_struct(php_http_cookie_list_t *list, zval *strct TSRMLS_DC);
PHP_HTTP_API void php_http_cookie_list_to_struct(php_http_cookie_list_t *list, zval *strct);


zend_class_entry *php_http_cookie_get_class_entry(void);

typedef struct php_http_cookie_object {
	zend_object o;
	php_http_cookie_list_t *list;
} php_http_cookie_object_t;

zend_object_value php_http_cookie_object_new(zend_class_entry *ce TSRMLS_DC);
zend_object_value php_http_cookie_object_new_ex(zend_class_entry *ce, php_http_cookie_list_t *list, php_http_cookie_object_t **obj TSRMLS_DC);
zend_object_value php_http_cookie_object_clone(zval *this_ptr TSRMLS_DC);
void php_http_cookie_object_free(void *object TSRMLS_DC);

PHP_METHOD(HttpCookie, __construct);
PHP_METHOD(HttpCookie, getCookies);
PHP_METHOD(HttpCookie, setCookies);
PHP_METHOD(HttpCookie, addCookies);
PHP_METHOD(HttpCookie, getExtras);
PHP_METHOD(HttpCookie, setExtras);
PHP_METHOD(HttpCookie, addExtras);
PHP_METHOD(HttpCookie, getCookie);
PHP_METHOD(HttpCookie, setCookie);
PHP_METHOD(HttpCookie, addCookie);
PHP_METHOD(HttpCookie, getExtra);
PHP_METHOD(HttpCookie, setExtra);
PHP_METHOD(HttpCookie, addExtra);
PHP_METHOD(HttpCookie, getDomain);
PHP_METHOD(HttpCookie, setDomain);
PHP_METHOD(HttpCookie, getPath);
PHP_METHOD(HttpCookie, setPath);
PHP_METHOD(HttpCookie, getExpires);
PHP_METHOD(HttpCookie, setExpires);
PHP_METHOD(HttpCookie, getMaxAge);
PHP_METHOD(HttpCookie, setMaxAge);
PHP_METHOD(HttpCookie, getFlags);
PHP_METHOD(HttpCookie, setFlags);
PHP_METHOD(HttpCookie, toString);
PHP_METHOD(HttpCookie, toArray);

extern PHP_MINIT_FUNCTION(http_cookie);


#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
