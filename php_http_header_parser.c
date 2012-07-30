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

typedef struct php_http_header_parser_state_spec {
	php_http_header_parser_state_t state;
	unsigned need_data:1;
} php_http_header_parser_state_spec_t;

static const php_http_header_parser_state_spec_t php_http_header_parser_states[] = {
		{PHP_HTTP_HEADER_PARSER_STATE_START,		1},
		{PHP_HTTP_HEADER_PARSER_STATE_KEY,			1},
		{PHP_HTTP_HEADER_PARSER_STATE_VALUE,		1},
		{PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX,		1},
		{PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE,	0},
		{PHP_HTTP_HEADER_PARSER_STATE_DONE,			0}
};


PHP_HTTP_API php_http_header_parser_t *php_http_header_parser_init(php_http_header_parser_t *parser TSRMLS_DC)
{
	if (!parser) {
		parser = emalloc(sizeof(*parser));
	}
	memset(parser, 0, sizeof(*parser));

	TSRMLS_SET_CTX(parser->ts);

	return parser;
}


PHP_HTTP_API php_http_header_parser_state_t php_http_header_parser_state_push(php_http_header_parser_t *parser, unsigned argc, ...)
{
	va_list va_args;
	unsigned i;
	php_http_header_parser_state_t state = 0;

	va_start(va_args, argc);
	for (i = 0; i < argc; ++i) {
		state = va_arg(va_args, php_http_header_parser_state_t);
		zend_stack_push(&parser->stack, &state, sizeof(state));
	}
	va_end(va_args);

	return state;
}

PHP_HTTP_API php_http_header_parser_state_t php_http_header_parser_state_is(php_http_header_parser_t *parser)
{
	php_http_header_parser_state_t *state;

	if (SUCCESS == zend_stack_top(&parser->stack, (void *) &state)) {
		return *state;
	}
	return PHP_HTTP_HEADER_PARSER_STATE_START;
}

PHP_HTTP_API php_http_header_parser_state_t php_http_header_parser_state_pop(php_http_header_parser_t *parser)
{
	php_http_header_parser_state_t state, *state_ptr;
	if (SUCCESS == zend_stack_top(&parser->stack, (void *) &state_ptr)) {
		state = *state_ptr;
		zend_stack_del_top(&parser->stack);
		return state;
	}
	return PHP_HTTP_HEADER_PARSER_STATE_START;
}

PHP_HTTP_API void php_http_header_parser_dtor(php_http_header_parser_t *parser)
{
	zend_stack_destroy(&parser->stack);
	php_http_info_dtor(&parser->info);
	STR_FREE(parser->_key.str);
	STR_FREE(parser->_val.str);
}

PHP_HTTP_API void php_http_header_parser_free(php_http_header_parser_t **parser)
{
	if (*parser) {
		php_http_header_parser_dtor(*parser);
		efree(*parser);
		*parser = NULL;
	}
}

PHP_HTTP_API STATUS php_http_header_parser_parse(php_http_header_parser_t *parser, php_http_buffer_t *buffer, unsigned flags, HashTable *headers, php_http_info_callback_t callback_func, void *callback_arg)
{
	TSRMLS_FETCH_FROM_CTX(parser->ts);

	while (buffer->used || !php_http_header_parser_states[php_http_header_parser_state_is(parser)].need_data) {
#if 0
		const char *state[] = {"START", "KEY", "VALUE", "HEADER_DONE", "DONE"};
		fprintf(stderr, "#HP: %s (avail:%zu, num:%d)\n", php_http_header_parser_state_is(parser) < 0 ? "FAILURE" : state[php_http_header_parser_state_is(parser)], buffer->used, headers?zend_hash_num_elements(headers):0);
		_dpf(0, buffer->data, buffer->used);
#endif
		switch (php_http_header_parser_state_pop(parser)) {
			case PHP_HTTP_HEADER_PARSER_STATE_FAILURE:
				return php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_FAILURE);

			case PHP_HTTP_HEADER_PARSER_STATE_START: {
				char *ptr = buffer->data;

				while (ptr - buffer->data < buffer->used && PHP_HTTP_IS_CTYPE(space, *ptr)) {
					++ptr;
				}

				php_http_buffer_cut(buffer, 0, ptr - buffer->data);
				php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_KEY);
				break;
			}

			case PHP_HTTP_HEADER_PARSER_STATE_KEY: {
				const char *colon, *eol_str = NULL;
				int eol_len = 0;

				if (buffer->data == (eol_str = php_http_locate_bin_eol(buffer->data, buffer->used, &eol_len))) {
					/* end of headers */
					php_http_buffer_cut(buffer, 0, eol_len);
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_DONE);
				} else if (php_http_info_parse(&parser->info, php_http_buffer_fix(buffer)->data TSRMLS_CC)) {
					/* new message starting with request/response line */
					if (callback_func) {
						callback_func(callback_arg, &headers, &parser->info TSRMLS_CC);
					}
					php_http_info_dtor(&parser->info);
					php_http_buffer_cut(buffer, 0, eol_str + eol_len - buffer->data);
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
				} else if ((colon = memchr(buffer->data, ':', buffer->used)) && (!eol_str || eol_str > colon)) {
					/* header: string */
					parser->_key.str = estrndup(buffer->data, parser->_key.len = colon - buffer->data);
					while (PHP_HTTP_IS_CTYPE(space, *++colon) && *colon != '\n' && *colon != '\r');
					php_http_buffer_cut(buffer, 0, colon - buffer->data);
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_VALUE);
				} else {
					/* neither reqeust/response line nor header: string */
					return php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_FAILURE);
				}
				break;
			}

			case PHP_HTTP_HEADER_PARSER_STATE_VALUE: {
				const char *eol_str;
				int eol_len;

#define SET_ADD_VAL(slen, eol_len) \
	do { \
		char *ptr = buffer->data; \
		size_t len = slen; \
		 \
		while (len > 0 && PHP_HTTP_IS_CTYPE(space, *ptr)) { \
			++ptr; \
			--len; \
		} \
		while (len > 0 && PHP_HTTP_IS_CTYPE(space, ptr[len - 1])) { \
			--len; \
		} \
		 \
		if (len > 0) { \
			if (parser->_val.str) { \
				parser->_val.str = erealloc(parser->_val.str, parser->_val.len + len + 2); \
				parser->_val.str[parser->_val.len++] = ' '; \
				memcpy(&parser->_val.str[parser->_val.len], ptr, len); \
				parser->_val.len += len; \
				parser->_val.str[parser->_val.len] = '\0'; \
			} else { \
				parser->_val.len = len; \
				parser->_val.str = estrndup(ptr, len); \
			} \
		} \
		php_http_buffer_cut(buffer, 0, slen + eol_len); \
	} while (0)

				if ((eol_str = php_http_locate_bin_eol(buffer->data, buffer->used, &eol_len))) {
					SET_ADD_VAL(eol_str - buffer->data, eol_len);

					if (buffer->used) {
						if (*buffer->data != '\t' && *buffer->data != ' ') {
							php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
							break;
						} else {
							php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_VALUE);
							break;
						}
					}
				}

				if (flags & PHP_HTTP_HEADER_PARSER_CLEANUP) {
					if (buffer->used) {
						SET_ADD_VAL(buffer->used, 0);
					}
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
				} else {
					return php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX);
				}
				break;
			}

			case PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX:
				if (*buffer->data == ' ' || *buffer->data == '\t') {
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_VALUE);
				} else {
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
				}
				break;

			case PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE:
				if (parser->_key.str && parser->_val.str) {
					zval array, **exist;

					if (!headers && callback_func) {
						callback_func(callback_arg, &headers, NULL TSRMLS_CC);
					}

					INIT_PZVAL_ARRAY(&array, headers);
					php_http_pretty_key(parser->_key.str, parser->_key.len, 1, 1);
					if (SUCCESS == zend_symtable_find(headers, parser->_key.str, parser->_key.len + 1, (void *) &exist)) {
						convert_to_array(*exist);
						add_next_index_stringl(*exist, parser->_val.str, parser->_val.len, 0);
					} else {
						add_assoc_stringl_ex(&array, parser->_key.str, parser->_key.len + 1, parser->_val.str, parser->_val.len, 0);
					}
					parser->_val.str = NULL;
				}

				STR_SET(parser->_key.str, NULL);
				STR_SET(parser->_val.str, NULL);

				php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_KEY);
				break;

			case PHP_HTTP_HEADER_PARSER_STATE_DONE:
				return PHP_HTTP_HEADER_PARSER_STATE_DONE;
		}
	}

	return php_http_header_parser_state_is(parser);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

