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
	zend_ptr_stack stack;
	php_http_info_t info;
	struct {
		char *str;
		size_t len;
	} _key;
	struct {
		char *str;
		size_t len;
	} _val;
} php_http_header_parser_t;

PHP_HTTP_API php_http_header_parser_t *php_http_header_parser_init(php_http_header_parser_t *parser);
PHP_HTTP_API php_http_header_parser_state_t php_http_header_parser_state_is(php_http_header_parser_t *parser);
PHP_HTTP_API void php_http_header_parser_dtor(php_http_header_parser_t *parser);
PHP_HTTP_API void php_http_header_parser_free(php_http_header_parser_t **parser);
PHP_HTTP_API php_http_header_parser_state_t php_http_header_parser_parse(php_http_header_parser_t *parser, php_http_buffer_t *buffer, unsigned flags, HashTable *headers, php_http_info_callback_t callback_func, void *callback_arg);
PHP_HTTP_API php_http_header_parser_state_t php_http_headerparser_parse_stream(php_http_header_parser_t *parser, php_http_buffer_t *buffer, php_stream *s, unsigned flags, HashTable *headers, php_http_info_callback_t callback_func, void *callback_arg);

typedef struct php_http_header_parser_object {
	php_http_buffer_t *buffer;
	php_http_header_parser_t *parser;
	zend_object zo;
} php_http_header_parser_object_t;

PHP_HTTP_API zend_class_entry *php_http_get_header_parser_class_entry(void);

PHP_MINIT_FUNCTION(http_header_parser);

zend_object *php_http_header_parser_object_new(zend_class_entry *ce);
php_http_header_parser_object_t *php_http_header_parser_object_new_ex(zend_class_entry *ce, php_http_header_parser_t *parser);
void php_http_header_parser_object_free(zend_object *object);

#endif /* PHP_HTTP_HEADER_PARSER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

