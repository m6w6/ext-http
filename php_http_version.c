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

#include "php_http_api.h"

PHP_HTTP_API php_http_version_t *php_http_version_init(php_http_version_t *v, unsigned major, unsigned minor TSRMLS_DC)
{
	if (!v) {
		v = emalloc(sizeof(*v));
	}

	v->major = major;
	v->minor = minor;

	return v;
}

PHP_HTTP_API php_http_version_t *php_http_version_parse(php_http_version_t *v, const char *str TSRMLS_DC)
{
	php_http_version_t tmp;
	char separator = 0;

	if (3 != sscanf(str, "HTTP/%u%c%u", &tmp.major, &separator, &tmp.minor)
	&&	3 != sscanf(str, "%u%c%u", &tmp.major, &separator, &tmp.minor)) {
		php_http_error(HE_WARNING, PHP_HTTP_E_MALFORMED_HEADERS, "Could not parse HTTP protocol version '%s'", str);
		return NULL;
	}

	if (separator && separator != '.' && separator != ',') {
		php_http_error(HE_NOTICE, PHP_HTTP_E_MALFORMED_HEADERS, "Non-standard version separator '%c' in HTTP protocol version '%s'", separator, str);
	}

	return php_http_version_init(v, tmp.major, tmp.minor TSRMLS_CC);
}

PHP_HTTP_API void php_http_version_to_string(php_http_version_t *v, char **str, size_t *len, const char *pre, const char *post TSRMLS_DC)
{
	*len = spprintf(str, 0, "%s%u.%u%s", pre ? pre : "", v->major, v->minor, post ? post : "");
}

PHP_HTTP_API void php_http_version_dtor(php_http_version_t *v)
{
	(void) v;
}

PHP_HTTP_API void php_http_version_free(php_http_version_t **v)
{
	if (*v) {
		php_http_version_dtor(*v);
		efree(*v);
		*v = NULL;
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

