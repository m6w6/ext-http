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

#ifndef PHP_HTTP_MESSAGE_PARSER_H
#define PHP_HTTP_MESSAGE_PARSER_H

#include "php_http_header_parser.h"
#include "php_http_encoding.h"
#include "php_http_message.h"

typedef enum php_http_message_parser_state {
	PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE = FAILURE,
	PHP_HTTP_MESSAGE_PARSER_STATE_START = 0,
	PHP_HTTP_MESSAGE_PARSER_STATE_HEADER,
	PHP_HTTP_MESSAGE_PARSER_STATE_HEADER_DONE,
	PHP_HTTP_MESSAGE_PARSER_STATE_BODY,
	PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DUMB,
	PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH,
	PHP_HTTP_MESSAGE_PARSER_STATE_BODY_CHUNKED,
	PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE,
	PHP_HTTP_MESSAGE_PARSER_STATE_DONE
} php_http_message_parser_state_t;

#define PHP_HTTP_MESSAGE_PARSER_CLEANUP			0x1
#define PHP_HTTP_MESSAGE_PARSER_DUMB_BODIES		0x2
#define PHP_HTTP_MESSAGE_PARSER_EMPTY_REDIRECTS	0x4
#define PHP_HTTP_MESSAGE_PARSER_GREEDY			0x8

typedef struct php_http_message_parser {
	php_http_header_parser_t header;
	zend_stack stack;
	size_t body_length;
	php_http_message_t *message;
	php_http_encoding_stream_t *dechunk;
	php_http_encoding_stream_t *inflate;
#ifdef ZTS
	void ***ts;
#endif
} php_http_message_parser_t;

PHP_HTTP_API php_http_message_parser_t *php_http_message_parser_init(php_http_message_parser_t *parser TSRMLS_DC);
PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_state_push(php_http_message_parser_t *parser, unsigned argc, ...);
PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_state_is(php_http_message_parser_t *parser);
PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_state_pop(php_http_message_parser_t *parser);
PHP_HTTP_API void php_http_message_parser_dtor(php_http_message_parser_t *parser);
PHP_HTTP_API void php_http_message_parser_free(php_http_message_parser_t **parser);
PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_parse(php_http_message_parser_t *parser, php_http_buffer_t *buffer, unsigned flags, php_http_message_t **message);
PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_parse_stream(php_http_message_parser_t *parser, php_stream *s, unsigned flags, php_http_message_t **message);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

