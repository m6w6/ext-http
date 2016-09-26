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

#ifndef PHP_HTTP_URL_H
#define PHP_HTTP_URL_H

#include "ext/standard/url.h"

/* php_http_url_mod() */
#define PHP_HTTP_URL_REPLACE		0x000
#define PHP_HTTP_URL_JOIN_PATH		0x001
#define PHP_HTTP_URL_JOIN_QUERY		0x002
#define PHP_HTTP_URL_STRIP_USER		0x004
#define PHP_HTTP_URL_STRIP_PASS		0x008
#define PHP_HTTP_URL_STRIP_AUTH		(PHP_HTTP_URL_STRIP_USER|PHP_HTTP_URL_STRIP_PASS)
#define PHP_HTTP_URL_STRIP_PORT		0x020
#define PHP_HTTP_URL_STRIP_PATH		0x040
#define PHP_HTTP_URL_STRIP_QUERY	0x080
#define PHP_HTTP_URL_STRIP_FRAGMENT	0x100
#define PHP_HTTP_URL_STRIP_ALL ( \
	PHP_HTTP_URL_STRIP_AUTH | \
	PHP_HTTP_URL_STRIP_PORT | \
	PHP_HTTP_URL_STRIP_PATH | \
	PHP_HTTP_URL_STRIP_QUERY | \
	PHP_HTTP_URL_STRIP_FRAGMENT \
)
#define PHP_HTTP_URL_FROM_ENV		0x1000
#define PHP_HTTP_URL_SANITIZE_PATH	0x2000

/* parse multibyte according to locale */
#define PHP_HTTP_URL_PARSE_MBLOC	0x10000
/* parse utf8 multibyte sequences */
#define PHP_HTTP_URL_PARSE_MBUTF8	0x20000
/* convert multibyte hostnames to IDNA */
#define PHP_HTTP_URL_PARSE_TOIDN	0x100000
/* percent encode multibyte sequences in userinfo, path, query and fragment */
#define PHP_HTTP_URL_PARSE_TOPCT	0x200000

#if PHP_HTTP_HAVE_IDNA2008
#define PHP_HTTP_URL_PARSE_TOIDN_2008 \
	(PHP_HTTP_URL_PARSE_TOIDN	|	0x400000)
#endif
#if PHP_HTTP_HAVE_IDNA2003
#define PHP_HTTP_URL_PARSE_TOIDN_2003 \
	(PHP_HTTP_URL_PARSE_TOIDN	|	0x800000)
#endif

/* ignore errors */
#define PHP_HTTP_URL_IGNORE_ERRORS	0x10000000
/* do not report errors */
#define PHP_HTTP_URL_SILENT_ERRORS	0x20000000

#define PHP_HTTP_URL_STDFLAGS		0x00332003

typedef struct php_http_url {
	/* compatible to php_url, but do not use php_url_free() */
	char *scheme;
	char *user;
	char *pass;
	char *host;
	unsigned short port;
	char *path;
	char *query;
	char *fragment;
} php_http_url_t;

PHP_HTTP_API php_http_url_t *php_http_url_parse(const char *str, size_t len, unsigned flags);
PHP_HTTP_API php_http_url_t *php_http_url_parse_authority(const char *str, size_t len, unsigned flags);
PHP_HTTP_API php_http_url_t *php_http_url_mod(const php_http_url_t *old_url, const php_http_url_t *new_url, unsigned flags);
PHP_HTTP_API php_http_url_t *php_http_url_copy(const php_http_url_t *url, zend_bool persistent);
PHP_HTTP_API php_http_url_t *php_http_url_from_struct(HashTable *ht);
PHP_HTTP_API php_http_url_t *php_http_url_from_zval(zval *value, unsigned flags);
PHP_HTTP_API HashTable *php_http_url_to_struct(const php_http_url_t *url, zval *strct);
PHP_HTTP_API char *php_http_url_to_string(const php_http_url_t *url, char **url_str, size_t *url_len, zend_bool persistent);
PHP_HTTP_API char *php_http_url_authority_to_string(const php_http_url_t *url, char **url_str, size_t *url_len);
PHP_HTTP_API void php_http_url_free(php_http_url_t **url);

PHP_HTTP_API ZEND_RESULT_CODE php_http_url_encode_hash(HashTable *hash, const char *pre_encoded_str, size_t pre_encoded_len, char **encoded_str, size_t *encoded_len);
PHP_HTTP_API ZEND_RESULT_CODE php_http_url_encode_hash_ex(HashTable *hash, php_http_buffer_t *qstr, const char *arg_sep_str, size_t arg_sep_len, const char *val_sep_str, size_t val_sep_len, const char *pre_encoded_str, size_t pre_encoded_len);

static inline void php_http_url_argsep(const char **str, size_t *len)
{
	if (SUCCESS != php_http_ini_entry(ZEND_STRL("arg_separator.output"), str, len, 0) || !*len) {
		*str = PHP_HTTP_URL_ARGSEP;
		*len = lenof(PHP_HTTP_URL_ARGSEP);
	}
}

static inline zend_bool php_http_url_is_empty(const php_http_url_t *url) {
	return !(url->scheme || url->pass || url->user || url->host || url->port ||	url->path || url->query || url->fragment);
}

PHP_HTTP_API zend_class_entry *php_http_url_get_class_entry(void);
PHP_HTTP_API zend_class_entry *php_http_get_env_url_class_entry(void);
PHP_MINIT_FUNCTION(http_url);

#define php_http_url_object_new php_http_object_new
#define php_http_url_object_new_ex php_http_object_new_ex

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

