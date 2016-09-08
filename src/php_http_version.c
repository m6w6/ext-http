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

php_http_version_t *php_http_version_init(php_http_version_t *v, unsigned major, unsigned minor)
{
	if (!v) {
		v = emalloc(sizeof(*v));
	}

	v->major = major;
	v->minor = minor;

	return v;
}

php_http_version_t *php_http_version_parse(php_http_version_t *v, const char *str)
{
	long major, minor;
	char separator = 0;
	register const char *ptr = str;

	switch (*ptr) {
	case 'h':
	case 'H':
		++ptr; if (*ptr != 't' && *ptr != 'T') break;
		++ptr; if (*ptr != 't' && *ptr != 'T') break;
		++ptr; if (*ptr != 'p' && *ptr != 'P') break;
		++ptr; if (*ptr != '/') break;
		++ptr;
		/* no break */
	default:
		/* rfc7230#2.6 The HTTP version number consists of two decimal digits separated by a "." (period or decimal point) */
		major = *ptr++ - '0';
		if (major >= 0 && major <= 9) {
			separator = *ptr++;
			switch (separator) {
			default:
				php_error_docref(NULL, E_NOTICE, "Non-standard version separator '%c' in HTTP protocol version '%s'", separator, ptr - 2);
				/* no break */
			case '.':
			case ',':
				minor = *ptr - '0';
				break;

			case ' ':
				if (major > 1) {
					minor = 0;
				} else {
					goto error;
				}
			}
			if (minor >= 0 && minor <= 9) {
				return php_http_version_init(v, major, minor);
			}
		}
	}

	error:
	php_error_docref(NULL, E_WARNING, "Could not parse HTTP protocol version '%s'", str);
	return NULL;
}

void php_http_version_to_string(php_http_version_t *v, char **str, size_t *len, const char *pre, const char *post)
{
	/* different semantics for different versions */
	if (v->major == 2) {
		*len = spprintf(str, 0, "%s2%s", STR_PTR(pre), STR_PTR(post));
	} else  {
		*len = spprintf(str, 0, "%s%u.%u%s", STR_PTR(pre), v->major, v->minor, STR_PTR(post));
	}
}

void php_http_version_dtor(php_http_version_t *v)
{
	(void) v;
}

void php_http_version_free(php_http_version_t **v)
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

