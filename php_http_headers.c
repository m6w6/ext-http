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

#include "php_http.h"

PHP_HTTP_API STATUS php_http_headers_parse(const char *header, size_t length, HashTable *headers, php_http_info_callback_t callback_func, void **callback_data TSRMLS_DC)
{
	php_http_header_parser_t ctx;
	php_http_buffer_t buf;
	
	php_http_buffer_from_string_ex(&buf, header, length);
	php_http_header_parser_init(&ctx TSRMLS_CC);
	php_http_header_parser_parse(&ctx, &buf, PHP_HTTP_HEADER_PARSER_CLEANUP, headers, callback_func, callback_data);
	php_http_header_parser_dtor(&ctx);
	php_http_buffer_dtor(&buf);
	/* FIXME */
	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

