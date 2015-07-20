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

#ifndef PHP_HTTP_VERSION_H
#define	PHP_HTTP_VERSION_H

typedef struct php_http_version {
	unsigned major;
	unsigned minor;
} php_http_version_t;

PHP_HTTP_API php_http_version_t *php_http_version_init(php_http_version_t *v, unsigned major, unsigned minor);
PHP_HTTP_API php_http_version_t *php_http_version_parse(php_http_version_t *v, const char *str);
PHP_HTTP_API void php_http_version_to_string(php_http_version_t *v, char **str, size_t *len, const char *pre, const char *post);
PHP_HTTP_API void php_http_version_dtor(php_http_version_t *v);
PHP_HTTP_API void php_http_version_free(php_http_version_t **v);

#endif	/* PHP_HTTP_VERSION_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

