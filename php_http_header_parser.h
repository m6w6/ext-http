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

#ifndef PHP_HTTP_HEADER_PARSER_H
#define PHP_HTTP_HEADER_PARSER_H

#include "php_http_info.h"

typedef enum php_http_header_parser_state {
	PHP_HTTP_HEADER_PARSER_STATE_FAILURE = FAILURE,
	PHP_HTTP_HEADER_PARSER_STATE_START = 0,
	PHP_HTTP_HEADER_PARSER_STATE_KEY,
	PHP_HTTP_HEADER_PARSER_STATE_VALUE,
	PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX,
	PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE,
	PHP_HTTP_HEADER_PARSER_STATE_DONE
} php_http_header_parser_state_t;

#define PHP_HTTP_HEADER_PARSER_CLEANUP 0x1

typedef struct php_http_header_parser {
	zend_stack stack;
	php_http_info_t info;
	struct {
		char *str;
		size_t len;
	} _key;
	struct {
		char *str;
		size_t len;
	} _val;
#ifdef ZTS
	void ***ts;
#endif
} php_http_header_parser_t;

PHP_HTTP_API php_http_header_parser_t *php_http_header_parser_init(php_http_header_parser_t *parser TSRMLS_DC);
PHP_HTTP_API php_http_header_parser_state_t php_http_header_parser_state_push(php_http_header_parser_t *parser, unsigned argc, ...);
PHP_HTTP_API php_http_header_parser_state_t php_http_header_parser_state_is(php_http_header_parser_t *parser);
PHP_HTTP_API php_http_header_parser_state_t php_http_header_parser_state_pop(php_http_header_parser_t *parser);
PHP_HTTP_API void php_http_header_parser_dtor(php_http_header_parser_t *parser);
PHP_HTTP_API void php_http_header_parser_free(php_http_header_parser_t **parser);
PHP_HTTP_API php_http_header_parser_state_t php_http_header_parser_parse(php_http_header_parser_t *parser, php_http_buffer_t *buffer, unsigned flags, HashTable *headers, php_http_info_callback_t callback_func, void *callback_arg);

#endif /* PHP_HTTP_HEADER_PARSER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

