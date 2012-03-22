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

PHP_HTTP_API int php_http_request_method_is(const char *meth, int argc, ...)
{
	va_list argv;
	int match = -1;

	if (argc > 0) {
		int i;

		va_start(argv, argc);
		for (i = 0; i < argc; ++i) {
			const char *test = va_arg(argv, const char *);

			if (!strcasecmp(meth, test)) {
				match = i;
				break;
			}
		}
		va_end(argv);
	}

	return match;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

