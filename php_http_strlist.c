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

PHP_HTTP_API php_http_strlist_iterator_t *php_http_strlist_iterator_init(php_http_strlist_iterator_t *iter, const char list[], unsigned factor)
{
	if (!iter) {
		iter = emalloc(sizeof(*iter));
	}
	memset(iter, 0, sizeof(*iter));

	iter->p = &list[0];
	iter->factor = factor;

	return iter;
}

PHP_HTTP_API const char *php_http_strlist_iterator_this(php_http_strlist_iterator_t *iter, unsigned *id)
{
	if (id) {
		*id = (iter->major + 1) * iter->factor + iter->minor;
	}

	return iter->p;
}

PHP_HTTP_API const char *php_http_strlist_iterator_next(php_http_strlist_iterator_t *iter)
{
	if (*iter->p) {
		while (*iter->p) {
			++iter->p;
		}
		++iter->p;
		++iter->minor;

		if (!*iter->p) {
			++iter->p;
			++iter->major;
			iter->minor = 0;
		}
	}

    return iter->p;
}

PHP_HTTP_API void php_http_strlist_iterator_dtor(php_http_strlist_iterator_t *iter)
{

}

PHP_HTTP_API void php_http_strlist_iterator_free(php_http_strlist_iterator_t **iter)
{
	if (*iter) {
		efree(*iter);
		*iter = NULL;
	}
}

PHP_HTTP_API const char *php_http_strlist_find(const char list[], unsigned factor, unsigned item)
{
	unsigned M = 0, m = 0, major, minor;
	const char *p = &list[0];

	if (factor) {
		major = (item / factor) - 1;
		minor = item % factor;
	} else {
		major = 0;
		minor = item;
	}
    while (*p && major != M++) {
        while (*p) {
            while (*p) {
                ++p;
            }
            ++p;
        }
        ++p;
    }

    while (*p && minor != m++) {
        while (*p) {
            ++p;
        }
        ++p;
    }

    return p;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

