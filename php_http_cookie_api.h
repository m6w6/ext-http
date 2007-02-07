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

#ifndef PHP_HTTP_COOKIE_API_H
#define PHP_HTTP_COOKIE_API_H

#define HTTP_COOKIE_SECURE		0x10L
#define HTTP_COOKIE_HTTPONLY	0x20L

#define HTTP_COOKIE_PARSE_RAW	0x01L

extern PHP_MINIT_FUNCTION(http_cookie);

/*
	generally a netscape cookie compliant struct, recognizing httpOnly attribute, too;
	cookie params like those from rfc2109 and rfc2965 are just put into extras, if
	one specifies them in allowed extras, else they're treated like cookies themself
*/
typedef struct _http_cookie_list_t {
	HashTable cookies;
	HashTable extras;
	long flags;
	char *path;
	char *domain;
	time_t expires;
} http_cookie_list;

#define http_cookie_list_new() _http_cookie_list_init(NULL ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
#define http_cookie_list_init(l) _http_cookie_list_init((l) ZEND_FILE_LINE_CC ZEND_FILE_LINE_EMPTY_CC TSRMLS_CC)
PHP_HTTP_API http_cookie_list *_http_cookie_list_init(http_cookie_list *list ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC);

#define http_cookie_list_dtor(l) _http_cookie_list_dtor((l) TSRMLS_CC)
PHP_HTTP_API void _http_cookie_list_dtor(http_cookie_list *list TSRMLS_DC);

#define http_cookie_list_free(l) _http_cookie_list_free((l) TSRMLS_CC)
PHP_HTTP_API void _http_cookie_list_free(http_cookie_list **list TSRMLS_DC);

#define http_cookie_list_has_cookie(list, name, name_len) zend_hash_exists(&(list)->cookies, (name), (name_len)+1)
#define http_cookie_list_has_extra(list, name, name_len) zend_hash_exists(&(list)->extras, (name), (name_len)+1)

#define http_cookie_list_add_cookie(l, n, nl, v, vl) _http_cookie_list_add_cookie((l), (n), (nl), (v), (vl) TSRMLS_CC)
PHP_HTTP_API void _http_cookie_list_add_cookie(http_cookie_list *list, const char *name, size_t name_len, const char *value, size_t value_len TSRMLS_DC);

#define http_cookie_list_add_extra(l, n , nl, v, vl) _http_cookie_list_add_extra((l), (n), (nl), (v), (vl) TSRMLS_CC)
PHP_HTTP_API void _http_cookie_list_add_extra(http_cookie_list *list, const char *name, size_t name_len, const char *value, size_t value_len TSRMLS_DC);

#define http_cookie_list_get_cookie(l, n, nl) _http_cookie_list_get_cookie((l), (n), (nl) TSRMLS_CC)
PHP_HTTP_API const char *_http_cookie_list_get_cookie(http_cookie_list *list, const char *name, size_t name_len TSRMLS_DC);

#define http_cookie_list_get_extra(l, n, nl) _http_cookie_list_get_extra((l), (n), (nl) TSRMLS_CC)
PHP_HTTP_API const char *_http_cookie_list_get_extra(http_cookie_list *list, const char *name, size_t name_len TSRMLS_DC);

#define http_parse_cookie(s) _http_parse_cookie_ex(NULL, (s), 0, NULL TSRMLS_CC)
#define http_parse_cookie_ex(l, s, f, a) _http_parse_cookie_ex((l), (s), (f), (a) TSRMLS_CC)
PHP_HTTP_API http_cookie_list *_http_parse_cookie_ex(http_cookie_list * list, const char *string, long flags, char **allowed_extras TSRMLS_DC);

#define http_cookie_list_tostruct(l, s) _http_cookie_list_tostruct((l), (s) TSRMLS_CC)
PHP_HTTP_API void _http_cookie_list_tostruct(http_cookie_list *list, zval *strct TSRMLS_DC);

#define http_cookie_list_fromstruct(l, s) _http_cookie_list_fromstruct((l), (s) TSRMLS_CC)
PHP_HTTP_API http_cookie_list *_http_cookie_list_fromstruct(http_cookie_list *list, zval *strct TSRMLS_DC);

#define http_cookie_list_tostring(l, str, len) _http_cookie_list_tostring((l), (str), (len) TSRMLS_CC)
PHP_HTTP_API void _http_cookie_list_tostring(http_cookie_list *list, char **str, size_t *len TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
