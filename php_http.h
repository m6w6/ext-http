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

#ifndef PHP_EXT_HTTP_H
#define PHP_EXT_HTTP_H

#define PHP_HTTP_EXT_VERSION "2.0.0dev"

extern zend_module_entry http_module_entry;
#define phpext_http_ptr &http_module_entry

extern int http_module_number;

#if PHP_DEBUG
#	define _DPF_STR	0
#	define _DPF_IN	1
#	define _DPF_OUT	2
extern void _dpf(int type, const char *data, size_t length);
#else
#	define _dpf(t,s,l);
#endif

#endif	/* PHP_EXT_HTTP_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

