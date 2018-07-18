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

php_http_header_parser_t *php_http_header_parser_init(php_http_header_parser_t *parser)
{
	if (!parser) {
		parser = emalloc(sizeof(*parser));
	}
	memset(parser, 0, sizeof(*parser));

	return parser;
}

#define php_http_header_parser_state_push(parser, state) zend_ptr_stack_push(&(parser)->stack, (void *) (state)), (state)
#define php_http_header_parser_state_ex(parser) ((parser)->stack.top \
		? (php_http_header_parser_state_t) (parser)->stack.elements[(parser)->stack.top - 1] \
		: PHP_HTTP_HEADER_PARSER_STATE_START)

php_http_header_parser_state_t php_http_header_parser_state_is(php_http_header_parser_t *parser)
{
	return php_http_header_parser_state_ex(parser);
}

#define php_http_header_parser_state_pop(parser) ((parser)->stack.top \
		? (php_http_header_parser_state_t) zend_ptr_stack_pop(&(parser)->stack) \
		: PHP_HTTP_HEADER_PARSER_STATE_START)

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
static void php_http_header_parser_error(size_t valid_len, char *str, size_t len, const char *eol_str )
{
	zend_string *escaped_str, *zstr_str = zend_string_init(str, len, 0);

#if PHP_VERSION_ID < 70300
	escaped_str = php_addcslashes(zstr_str, 1, ZEND_STRL("\x0..\x1F\x7F..\xFF"));
#else
	escaped_str = php_addcslashes(zstr_str, ZEND_STRL("\x0..\x1F\x7F..\xFF"));
	zend_string_release_ex(zstr_str, 0);
#endif

	if (valid_len != len && (!eol_str || (str+valid_len) != eol_str)) {
		php_error_docref(NULL, E_WARNING, "Failed to parse headers: unexpected character '\\%03o' at pos %zu of '%s'", str[valid_len], valid_len, escaped_str->val);
	} else if (eol_str) {
		php_error_docref(NULL, E_WARNING, "Failed to parse headers: unexpected end of line at pos %zu of '%s'", eol_str - str, escaped_str->val);
	} else {
		php_error_docref(NULL, E_WARNING, "Failed to parse headers: unexpected end of input at pos %zu of '%s'", len, escaped_str->val);
	}

	efree(escaped_str);
}

php_http_header_parser_state_t php_http_header_parser_parse(php_http_header_parser_t *parser, php_http_buffer_t *buffer, unsigned flags, HashTable *headers, php_http_info_callback_t callback_func, void *callback_arg)
{
	while (buffer->used || !php_http_header_parser_states[php_http_header_parser_state_ex(parser)].need_data) {
#if DBG_PARSER
		const char *state[] = {"START", "KEY", "VALUE", "VALUE_EX", "HEADER_DONE", "DONE"};
		fprintf(stderr, "#HP: %s (avail:%zu, num:%d cleanup:%u)\n", php_http_header_parser_state_is(parser) < 0 ? "FAILURE" : state[php_http_header_parser_state_is(parser)], buffer->used, headers?zend_hash_num_elements(headers):0, flags);
		_dpf(0, buffer->data, buffer->used);
#endif
		switch (php_http_header_parser_state_pop(parser)) {
			case PHP_HTTP_HEADER_PARSER_STATE_FAILURE:
				php_error_docref(NULL, E_WARNING, "Failed to parse headers");
				return php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_FAILURE);

			case PHP_HTTP_HEADER_PARSER_STATE_START: {
				char *ptr = buffer->data;

				while (ptr - buffer->data < buffer->used && PHP_HTTP_IS_CTYPE(space, *ptr)) {
					++ptr;
				}

				php_http_buffer_cut(buffer, 0, ptr - buffer->data);
				php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_KEY);
				break;
			}

			case PHP_HTTP_HEADER_PARSER_STATE_KEY: {
				const char *colon, *eol_str = NULL;
				int eol_len = 0;

				/* fix buffer here, so eol_str pointer doesn't become obsolete afterwards */
				php_http_buffer_fix(buffer);

				if (buffer->data == (eol_str = php_http_locate_bin_eol(buffer->data, buffer->used, &eol_len))) {
					/* end of headers */
					php_http_buffer_cut(buffer, 0, eol_len);
					php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_DONE);
				} else if (php_http_info_parse(&parser->info, buffer->data)) {
					/* new message starting with request/response line */
					if (callback_func) {
						callback_func(callback_arg, &headers, &parser->info);
					}
					php_http_info_dtor(&parser->info);
					php_http_buffer_cut(buffer, 0, eol_str + eol_len - buffer->data);
					php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
				} else if ((colon = memchr(buffer->data, ':', buffer->used)) && (!eol_str || eol_str > colon)) {
					/* header: string */
					size_t valid_len;

					parser->_key.len = colon - buffer->data;
					parser->_key.str = estrndup(buffer->data, parser->_key.len);

					valid_len = strspn(parser->_key.str, PHP_HTTP_HEADER_NAME_CHARS);
					if (valid_len != parser->_key.len) {
						php_http_header_parser_error(valid_len, parser->_key.str, parser->_key.len, eol_str);
						PTR_SET(parser->_key.str, NULL);
						return php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_FAILURE);
					}
					while (PHP_HTTP_IS_CTYPE(space, *++colon) && *colon != '\n' && *colon != '\r');
					php_http_buffer_cut(buffer, 0, colon - buffer->data);
					php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_VALUE);
				} else if (eol_str || (flags & PHP_HTTP_HEADER_PARSER_CLEANUP)) {
					/* neither reqeust/response line nor 'header:' string, or injected new line or NUL etc. */
					php_http_header_parser_error(strspn(buffer->data, PHP_HTTP_HEADER_NAME_CHARS), buffer->data, buffer->used, eol_str);
					return php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_FAILURE);
				} else {
					/* keep feeding */
					return php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_KEY);
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
					php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX);
				} else if (flags & PHP_HTTP_HEADER_PARSER_CLEANUP) {
					if (buffer->used) {
						SET_ADD_VAL(buffer->used, 0);
					}
					php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
				} else {
					return php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_VALUE);
				}
				break;
			}

			case PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX:
				if (buffer->used && (*buffer->data == ' ' || *buffer->data == '\t')) {
					php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_VALUE);
				} else if (buffer->used || (flags & PHP_HTTP_HEADER_PARSER_CLEANUP)) {
					php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
				} else {
					/* keep feeding */
					return php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX);
				}
				break;

			case PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE:
				if (parser->_key.str && parser->_val.str) {
					zval tmp, *exist;
					size_t valid_len = strlen(parser->_val.str);

					/* check for truncation */
					if (valid_len != parser->_val.len) {
						php_http_header_parser_error(valid_len, parser->_val.str, parser->_val.len, NULL);

						PTR_SET(parser->_key.str, NULL);
						PTR_SET(parser->_val.str, NULL);

						return php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_FAILURE);
					}

					if (!headers && callback_func) {
						callback_func(callback_arg, &headers, NULL);
					}

					php_http_pretty_key(parser->_key.str, parser->_key.len, 1, 1);
					if ((exist = zend_symtable_str_find(headers, parser->_key.str, parser->_key.len))) {
						convert_to_array(exist);
						add_next_index_str(exist, php_http_cs2zs(parser->_val.str, parser->_val.len));
					} else {
						ZVAL_STR(&tmp, php_http_cs2zs(parser->_val.str, parser->_val.len));
						zend_symtable_str_update(headers, parser->_key.str, parser->_key.len, &tmp);
					}
					parser->_val.str = NULL;
				}

				PTR_SET(parser->_key.str, NULL);
				PTR_SET(parser->_val.str, NULL);

				php_http_header_parser_state_push(parser, PHP_HTTP_HEADER_PARSER_STATE_KEY);
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

static zend_class_entry *php_http_header_parser_class_entry;
zend_class_entry *php_http_get_header_parser_class_entry(void)
{
	return php_http_header_parser_class_entry;
}
static zend_object_handlers php_http_header_parser_object_handlers;

zend_object *php_http_header_parser_object_new(zend_class_entry *ce)
{
	return &php_http_header_parser_object_new_ex(ce, NULL)->zo;
}

php_http_header_parser_object_t *php_http_header_parser_object_new_ex(zend_class_entry *ce, php_http_header_parser_t *parser)
{
	php_http_header_parser_object_t *o;

	o = ecalloc(1, sizeof(php_http_header_parser_object_t) + zend_object_properties_size(ce));
	zend_object_std_init(&o->zo, ce);
	object_properties_init(&o->zo, ce);

	if (parser) {
		o->parser = parser;
	} else {
		o->parser = php_http_header_parser_init(NULL);
	}
	o->buffer = php_http_buffer_new();

	o->zo.handlers = &php_http_header_parser_object_handlers;

	return o;
}

void php_http_header_parser_object_free(zend_object *object)
{
	php_http_header_parser_object_t *o = PHP_HTTP_OBJ(object, NULL);

	if (o->parser) {
		php_http_header_parser_free(&o->parser);
	}
	if (o->buffer) {
		php_http_buffer_free(&o->buffer);
	}
	zend_object_std_dtor(object);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpHeaderParser_getState, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpHeaderParser, getState)
{
	php_http_header_parser_object_t *parser_obj = PHP_HTTP_OBJ(NULL, getThis());

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
	size_t data_len;
	zend_long flags;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "slz", &data_str, &data_len, &flags, &zmsg), invalid_arg, return);

	ZVAL_DEREF(zmsg);
	if (Z_TYPE_P(zmsg) != IS_ARRAY) {
		zval_dtor(zmsg);
		array_init(zmsg);
	}
	parser_obj = PHP_HTTP_OBJ(NULL, getThis());
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
	zend_long flags;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "rlz", &zstream, &flags, &zmsg), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_get_exception_unexpected_val_class_entry(), &zeh);
	php_stream_from_zval(s, zstream);
	zend_restore_error_handling(&zeh);

	ZVAL_DEREF(zmsg);
	if (Z_TYPE_P(zmsg) != IS_ARRAY) {
		zval_dtor(zmsg);
		array_init(zmsg);
	}
	parser_obj = PHP_HTTP_OBJ(NULL, getThis());
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
	php_http_header_parser_class_entry = zend_register_internal_class(&ce);
	memcpy(&php_http_header_parser_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_header_parser_class_entry->create_object = php_http_header_parser_object_new;
	php_http_header_parser_object_handlers.offset = XtOffsetOf(php_http_header_parser_object_t, zo);
	php_http_header_parser_object_handlers.clone_obj = NULL;
	php_http_header_parser_object_handlers.free_obj = php_http_header_parser_object_free;

	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("CLEANUP"), PHP_HTTP_HEADER_PARSER_CLEANUP);

	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_FAILURE"), PHP_HTTP_HEADER_PARSER_STATE_FAILURE);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_START"), PHP_HTTP_HEADER_PARSER_STATE_START);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_KEY"), PHP_HTTP_HEADER_PARSER_STATE_KEY);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_VALUE"), PHP_HTTP_HEADER_PARSER_STATE_VALUE);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_VALUE_EX"), PHP_HTTP_HEADER_PARSER_STATE_VALUE_EX);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_HEADER_DONE"), PHP_HTTP_HEADER_PARSER_STATE_HEADER_DONE);
	zend_declare_class_constant_long(php_http_header_parser_class_entry, ZEND_STRL("STATE_DONE"), PHP_HTTP_HEADER_PARSER_STATE_DONE);

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

