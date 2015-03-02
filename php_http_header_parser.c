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

#ifndef DBG_PARSER
#	define DBG_PARSER 0
#endif

typedef struct php_http_header_parser_state_spec {
	php_http_header_parser_state_t state;
	unsigned need_data:1;
} php_http_header_parser_state_spec_t;

static const php_http_header_parser_state_spec_t php_http_header_parser_states[] = {
		{PHP_HTTP_HEADER_PARSER_STATE_START,		1},
		{PHP_HTTP_HEADER_PARSER_STATE_KEY,			1},
		{PHP_HTTP_HEADER_PARSER_STATE_VALUE,		1},
		{PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX,		0},
		{PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE,	0},
		{PHP_HTTP_HEADER_PARSER_STATE_DONE,			0}
};

php_http_header_parser_t *php_http_header_parser_init(php_http_header_parser_t *parser TSRMLS_DC)
{
	if (!parser) {
		parser = emalloc(sizeof(*parser));
	}
	memset(parser, 0, sizeof(*parser));

	TSRMLS_SET_CTX(parser->ts);

	return parser;
}

php_http_header_parser_state_t php_http_header_parser_state_push(php_http_header_parser_t *parser, unsigned argc, ...)
{
	va_list va_args;
	unsigned i;
	php_http_header_parser_state_t state = 0;

	/* short circuit */
	ZEND_PTR_STACK_RESIZE_IF_NEEDED((&parser->stack), argc);

	va_start(va_args, argc);
	for (i = 0; i < argc; ++i) {
		state = va_arg(va_args, php_http_header_parser_state_t);
		zend_ptr_stack_push(&parser->stack, (void *) state);
	}
	va_end(va_args);

	return state;
}

php_http_header_parser_state_t php_http_header_parser_state_is(php_http_header_parser_t *parser)
{
	if (parser->stack.top) {
		return (php_http_header_parser_state_t) parser->stack.elements[parser->stack.top - 1];
	}

	return PHP_HTTP_HEADER_PARSER_STATE_START;
}

php_http_header_parser_state_t php_http_header_parser_state_pop(php_http_header_parser_t *parser)
{
	if (parser->stack.top) {
		return (php_http_header_parser_state_t) zend_ptr_stack_pop(&parser->stack);
	}

	return PHP_HTTP_HEADER_PARSER_STATE_START;
}

void php_http_header_parser_dtor(php_http_header_parser_t *parser)
{
	zend_ptr_stack_destroy(&parser->stack);
	php_http_info_dtor(&parser->info);
	PTR_FREE(parser->_key.str);
	PTR_FREE(parser->_val.str);
}

void php_http_header_parser_free(php_http_header_parser_t **parser)
{
	if (*parser) {
		php_http_header_parser_dtor(*parser);
		efree(*parser);
		*parser = NULL;
	}
}

/* NOTE: 'str' has to be null terminated */
static void php_http_header_parser_error(size_t valid_len, char *str, size_t len, const char *eol_str TSRMLS_DC)
{
	int escaped_len;
	char *escaped_str;

	escaped_str = php_addcslashes(str, len, &escaped_len, 0, ZEND_STRL("\x0..\x1F\x7F..\xFF") TSRMLS_CC);

	if (valid_len != len && (!eol_str || (str+valid_len) != eol_str)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse headers: unexpected character '\\%03o' at pos %zu of '%.*s'", str[valid_len], valid_len, escaped_len, escaped_str);
	} else if (eol_str) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse headers: unexpected end of line at pos %zu of '%.*s'", eol_str - str, escaped_len, escaped_str);
	} else {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse headers: unexpected end of input at pos %zu of '%.*s'", len, escaped_len, escaped_str);
	}

	efree(escaped_str);
}

php_http_header_parser_state_t php_http_header_parser_parse(php_http_header_parser_t *parser, php_http_buffer_t *buffer, unsigned flags, HashTable *headers, php_http_info_callback_t callback_func, void *callback_arg)
{
	TSRMLS_FETCH_FROM_CTX(parser->ts);

	while (buffer->used || !php_http_header_parser_states[php_http_header_parser_state_is(parser)].need_data) {
#if DBG_PARSER
		const char *state[] = {"START", "KEY", "VALUE", "VALUE_EX", "HEADER_DONE", "DONE"};
		fprintf(stderr, "#HP: %s (avail:%zu, num:%d cleanup:%u)\n", php_http_header_parser_state_is(parser) < 0 ? "FAILURE" : state[php_http_header_parser_state_is(parser)], buffer->used, headers?zend_hash_num_elements(headers):0, flags);
		_dpf(0, buffer->data, buffer->used);
#endif
		switch (php_http_header_parser_state_pop(parser)) {
			case PHP_HTTP_HEADER_PARSER_STATE_FAILURE:
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Failed to parse headers");
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
					size_t valid_len;

					parser->_key.len = colon - buffer->data;
					parser->_key.str = estrndup(buffer->data, parser->_key.len);

					valid_len = strspn(parser->_key.str, PHP_HTTP_HEADER_NAME_CHARS);
					if (valid_len != parser->_key.len) {
						php_http_header_parser_error(valid_len, parser->_key.str, parser->_key.len, eol_str TSRMLS_CC);
						PTR_SET(parser->_key.str, NULL);
						return php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_FAILURE);
					}
					while (PHP_HTTP_IS_CTYPE(space, *++colon) && *colon != '\n' && *colon != '\r');
					php_http_buffer_cut(buffer, 0, colon - buffer->data);
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_VALUE);
				} else if (eol_str || (flags & PHP_HTTP_HEADER_PARSER_CLEANUP)) {
					/* neither reqeust/response line nor 'header:' string, or injected new line or NUL etc. */
					php_http_buffer_fix(buffer);
					php_http_header_parser_error(strspn(buffer->data, PHP_HTTP_HEADER_NAME_CHARS), buffer->data, buffer->used, eol_str TSRMLS_CC);
					return php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_FAILURE);
				} else {
					/* keep feeding */
					return php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_KEY);
				}
				break;
			}

			case PHP_HTTP_HEADER_PARSER_STATE_VALUE: {
				const char *eol_str;
				int eol_len;

#define SET_ADD_VAL(slen, eol_len) \
	do { \
		const char *ptr = buffer->data; \
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
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX);
				} else if (flags & PHP_HTTP_HEADER_PARSER_CLEANUP) {
					if (buffer->used) {
						SET_ADD_VAL(buffer->used, 0);
					}
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
				} else {
					return php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_VALUE);
				}
				break;
			}

			case PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX:
				if (buffer->used && (*buffer->data == ' ' || *buffer->data == '\t')) {
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_VALUE);
				} else if (buffer->used || (flags & PHP_HTTP_HEADER_PARSER_CLEANUP)) {
					php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
				} else {
					/* keep feeding */
					return php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX);
				}
				break;

			case PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE:
				if (parser->_key.str && parser->_val.str) {
					zval array, **exist;
					size_t valid_len = strlen(parser->_val.str);

					/* check for truncation */
					if (valid_len != parser->_val.len) {
						php_http_header_parser_error(valid_len, parser->_val.str, parser->_val.len, NULL TSRMLS_CC);

						PTR_SET(parser->_key.str, NULL);
						PTR_SET(parser->_val.str, NULL);

						return php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_FAILURE);
					}

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

				PTR_SET(parser->_key.str, NULL);
				PTR_SET(parser->_val.str, NULL);

				php_http_header_parser_state_push(parser, 1, PHP_HTTP_HEADER_PARSER_STATE_KEY);
				break;

			case PHP_HTTP_HEADER_PARSER_STATE_DONE:
				return PHP_HTTP_HEADER_PARSER_STATE_DONE;
		}
	}

	return php_http_header_parser_state_is(parser);
}

php_http_header_parser_state_t php_http_header_parser_parse_stream(php_http_header_parser_t *parser, php_http_buffer_t *buf, php_stream *s, unsigned flags, HashTable *headers, php_http_info_callback_t callback_func, void *callback_arg)
{
	php_http_header_parser_state_t state = PHP_HTTP_HEADER_PARSER_STATE_START;
	TSRMLS_FETCH_FROM_CTX(parser->ts);

	if (!buf->data) {
		php_http_buffer_resize_ex(buf, 0x1000, 1, 0);
	}
	while (1) {
		size_t justread = 0;
#if DBG_PARSER
		const char *states[] = {"START", "KEY", "VALUE", "VALUE_EX", "HEADER_DONE", "DONE"};
		fprintf(stderr, "#SHP: %s (f:%u)\n", states[state], flags);
#endif
		/* resize if needed */
		if (buf->free < 0x1000) {
			php_http_buffer_resize_ex(buf, 0x1000, 1, 0);
		}
		switch (state) {
		case PHP_HTTP_HEADER_PARSER_STATE_FAILURE:
		case PHP_HTTP_HEADER_PARSER_STATE_DONE:
			return state;

		default:
			/* read line */
			php_stream_get_line(s, buf->data + buf->used, buf->free, &justread);
			/* if we fail reading a whole line, try a single char */
			if (!justread) {
				int c = php_stream_getc(s);

				if (c != EOF) {
					char s[1] = {c};
					justread = php_http_buffer_append(buf, s, 1);
				}
			}
			php_http_buffer_account(buf, justread);
		}

		if (justread) {
			state = php_http_header_parser_parse(parser, buf, flags, headers, callback_func, callback_arg);
		} else if (php_stream_eof(s)) {
			return php_http_header_parser_parse(parser, buf, flags | PHP_HTTP_HEADER_PARSER_CLEANUP, headers, callback_func, callback_arg);
		} else  {
			return state;
		}
	}

	return PHP_HTTP_HEADER_PARSER_STATE_DONE;
}

zend_class_entry *php_http_header_parser_class_entry;
static zend_object_handlers php_http_header_parser_object_handlers;

zend_object_value php_http_header_parser_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_header_parser_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_header_parser_object_new_ex(zend_class_entry *ce, php_http_header_parser_t *parser, php_http_header_parser_object_t **ptr TSRMLS_DC)
{
	php_http_header_parser_object_t *o;

	o = ecalloc(1, sizeof(php_http_header_parser_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (ptr) {
		*ptr = o;
	}

	if (parser) {
		o->parser = parser;
	} else {
		o->parser = php_http_header_parser_init(NULL TSRMLS_CC);
	}
	o->buffer = php_http_buffer_new();

	o->zv.handle = zend_objects_store_put((zend_object *) o, NULL, php_http_header_parser_object_free, NULL TSRMLS_CC);
	o->zv.handlers = &php_http_header_parser_object_handlers;

	return o->zv;
}

void php_http_header_parser_object_free(void *object TSRMLS_DC)
{
	php_http_header_parser_object_t *o = (php_http_header_parser_object_t *) object;

	if (o->parser) {
		php_http_header_parser_free(&o->parser);
	}
	if (o->buffer) {
		php_http_buffer_free(&o->buffer);
	}
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeaderParser_getState, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpHeaderParser, getState)
{
	php_http_header_parser_object_t *parser_obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	zend_parse_parameters_none();
	/* always return the real state */
	RETVAL_LONG(php_http_header_parser_state_is(parser_obj->parser));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeaderParser_parse, 0, 0, 3)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, flags)
	ZEND_ARG_ARRAY_INFO(1, headers, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpHeaderParser, parse)
{
	php_http_header_parser_object_t *parser_obj;
	zval *zmsg;
	char *data_str;
	int data_len;
	long flags;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slz", &data_str, &data_len, &flags, &zmsg), invalid_arg, return);

	if (Z_TYPE_P(zmsg) != IS_ARRAY) {
		zval_dtor(zmsg);
		array_init(zmsg);
	}
	parser_obj = zend_object_store_get_object(getThis() TSRMLS_CC);
	php_http_buffer_append(parser_obj->buffer, data_str, data_len);
	RETVAL_LONG(php_http_header_parser_parse(parser_obj->parser, parser_obj->buffer, flags, Z_ARRVAL_P(zmsg), NULL, NULL));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeaderParser_stream, 0, 0, 3)
	ZEND_ARG_INFO(0, stream)
	ZEND_ARG_INFO(0, flags)
	ZEND_ARG_ARRAY_INFO(1, headers, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpHeaderParser, stream)
{
	php_http_header_parser_object_t *parser_obj;
	zend_error_handling zeh;
	zval *zmsg, *zstream;
	php_stream *s;
	long flags;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rlz", &zstream, &flags, &zmsg), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_exception_unexpected_val_class_entry, &zeh TSRMLS_CC);
	php_stream_from_zval(s, &zstream);
	zend_restore_error_handling(&zeh TSRMLS_CC);

	if (Z_TYPE_P(zmsg) != IS_ARRAY) {
		zval_dtor(zmsg);
		array_init(zmsg);
	}
	parser_obj = zend_object_store_get_object(getThis() TSRMLS_CC);
	RETVAL_LONG(php_http_header_parser_parse_stream(parser_obj->parser, parser_obj->buffer, s, flags, Z_ARRVAL_P(zmsg), NULL, NULL));
}

static zend_function_entry php_http_header_parser_methods[] = {
		PHP_ME(HttpHeaderParser, getState, ai_HttpHeaderParser_getState, ZEND_ACC_PUBLIC)
		PHP_ME(HttpHeaderParser, parse, ai_HttpHeaderParser_parse, ZEND_ACC_PUBLIC)
		PHP_ME(HttpHeaderParser, stream, ai_HttpHeaderParser_stream, ZEND_ACC_PUBLIC)
		{NULL, NULL, NULL}
};

PHP_MINIT_FUNCTION(http_header_parser)
{
	zend_class_entry ce;

	INIT_NS_CLASS_ENTRY(ce, "http\\Header", "Parser", php_http_header_parser_methods);
	php_http_header_parser_class_entry = zend_register_internal_class(&ce TSRMLS_CC);
	memcpy(&php_http_header_parser_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_header_parser_class_entry->create_object = php_http_header_parser_object_new;
	php_http_header_parser_object_handlers.clone_obj = NULL;

	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("CLEANUP"), PHP_HTTP_HEADER_PARSER_CLEANUP TSRMLS_CC);

	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_FAILURE"), PHP_HTTP_HEADER_PARSER_STATE_FAILURE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_START"), PHP_HTTP_HEADER_PARSER_STATE_START TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_KEY"), PHP_HTTP_HEADER_PARSER_STATE_KEY TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_VALUE"), PHP_HTTP_HEADER_PARSER_STATE_VALUE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_VALUE_EX"), PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_HEADER_DONE"), PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_DONE"), PHP_HTTP_HEADER_PARSER_STATE_DONE TSRMLS_CC);

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

