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

typedef struct php_http_message_parser_state_spec {
	php_http_message_parser_state_t state;
	unsigned need_data:1;
} php_http_message_parser_state_spec_t;

static const php_http_message_parser_state_spec_t php_http_message_parser_states[] = {
		{PHP_HTTP_MESSAGE_PARSER_STATE_START,			1},
		{PHP_HTTP_MESSAGE_PARSER_STATE_HEADER,			1},
		{PHP_HTTP_MESSAGE_PARSER_STATE_HEADER_DONE,		0},
		{PHP_HTTP_MESSAGE_PARSER_STATE_BODY,			0},
		{PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DUMB,		1},
		{PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH,		1},
		{PHP_HTTP_MESSAGE_PARSER_STATE_BODY_CHUNKED,	1},
		{PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE,		0},
		{PHP_HTTP_MESSAGE_PARSER_STATE_DONE,			0}
};

#if DBG_PARSER
const char *php_http_message_parser_state_name(php_http_message_parser_state_t state) {
	const char *states[] = {"START", "HEADER", "HEADER_DONE", "BODY", "BODY_DUMB", "BODY_LENGTH", "BODY_CHUNK", "BODY_DONE", "DONE"};
	
	if (state < 0 || state > (sizeof(states)/sizeof(char*))-1) {
		return "FAILURE";
	}
	return states[state];
}
#endif

php_http_message_parser_t *php_http_message_parser_init(php_http_message_parser_t *parser)
{
	if (!parser) {
		parser = emalloc(sizeof(*parser));
	}
	memset(parser, 0, sizeof(*parser));

	php_http_header_parser_init(&parser->header);

	return parser;
}

php_http_message_parser_state_t php_http_message_parser_state_push(php_http_message_parser_t *parser, unsigned argc, ...)
{
	php_http_message_parser_state_t state;
	va_list va_args;
	unsigned i;

	/* short circuit */
	ZEND_PTR_STACK_RESIZE_IF_NEEDED((&parser->stack), argc);

	va_start(va_args, argc);
	for (i = 0; i < argc; ++i) {
		state  = va_arg(va_args, php_http_message_parser_state_t);
		zend_ptr_stack_push(&parser->stack, (void *) state);
	}
	va_end(va_args);

	return state;
}

php_http_message_parser_state_t php_http_message_parser_state_is(php_http_message_parser_t *parser)
{
	if (parser->stack.top) {
		return (php_http_message_parser_state_t) parser->stack.elements[parser->stack.top - 1];
	}
	return PHP_HTTP_MESSAGE_PARSER_STATE_START;
}

php_http_message_parser_state_t php_http_message_parser_state_pop(php_http_message_parser_t *parser)
{
	if (parser->stack.top) {
		return (php_http_message_parser_state_t) zend_ptr_stack_pop(&parser->stack);
	}
	return PHP_HTTP_MESSAGE_PARSER_STATE_START;
}

void php_http_message_parser_dtor(php_http_message_parser_t *parser)
{
	php_http_header_parser_dtor(&parser->header);
	zend_ptr_stack_destroy(&parser->stack);
	php_http_message_free(&parser->message);
	if (parser->dechunk) {
		php_http_encoding_stream_free(&parser->dechunk);
	}
	if (parser->inflate) {
		php_http_encoding_stream_free(&parser->inflate);
	}
}

void php_http_message_parser_free(php_http_message_parser_t **parser)
{
	if (*parser) {
		php_http_message_parser_dtor(*parser);
		efree(*parser);
		*parser = NULL;
	}
}

php_http_message_parser_state_t php_http_message_parser_parse_stream(php_http_message_parser_t *parser, php_http_buffer_t *buf, php_stream *s, unsigned flags, php_http_message_t **message)
{
	php_http_message_parser_state_t state = PHP_HTTP_MESSAGE_PARSER_STATE_START;

	if (!buf->data) {
		php_http_buffer_resize_ex(buf, 0x1000, 1, 0);
	}

	while (!php_stream_eof(s)) {
		size_t justread = 0;
#if DBG_PARSER
		fprintf(stderr, "#SP: %s (f:%u)\n", php_http_message_parser_state_name(state), flags);
#endif
		switch (state) {
			case PHP_HTTP_MESSAGE_PARSER_STATE_START:
			case PHP_HTTP_MESSAGE_PARSER_STATE_HEADER:
			case PHP_HTTP_MESSAGE_PARSER_STATE_HEADER_DONE:
				/* read line */
				php_stream_get_line(s, buf->data + buf->used, buf->free, &justread);
				php_http_buffer_account(buf, justread);
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DUMB:
				/* read all */
				justread = php_stream_read(s, buf->data + buf->used, buf->free);
				php_http_buffer_account(buf, justread);
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH:
				/* read body_length */
				justread = php_stream_read(s, buf->data + buf->used, MIN(buf->free, parser->body_length));
				php_http_buffer_account(buf, justread);
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_CHUNKED:
				/* duh, this is very naive */
				if (parser->body_length) {
					justread = php_stream_read(s, buf->data + buf->used, MIN(parser->body_length, buf->free));

					php_http_buffer_account(buf, justread);

					parser->body_length -= justread;
				} else {
					php_http_buffer_resize(buf, 24);
					php_stream_get_line(s, buf->data, buf->free, &justread);
					php_http_buffer_account(buf, justread);

					parser->body_length = strtoul(buf->data + buf->used - justread, NULL, 16);
				}
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY:
			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE:
				/* should not occur */
				abort();
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_DONE:
			case PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE:
				return php_http_message_parser_state_is(parser);
		}

		if (justread) {
			state = php_http_message_parser_parse(parser, buf, flags, message);
		} else  {
			return state;
		}
	}

	return PHP_HTTP_MESSAGE_PARSER_STATE_DONE;
}


php_http_message_parser_state_t php_http_message_parser_parse(php_http_message_parser_t *parser, php_http_buffer_t *buffer, unsigned flags, php_http_message_t **message)
{
	char *str = NULL;
	size_t len = 0;
	size_t cut = 0;

	while (buffer->used || !php_http_message_parser_states[php_http_message_parser_state_is(parser)].need_data) {
#if DBG_PARSER
		fprintf(stderr, "#MP: %s (f: %u, t:%d, l:%zu)\n", 
			php_http_message_parser_state_name(php_http_message_parser_state_is(parser)),
			flags, 
			message && *message ? (*message)->type : -1, 
			buffer->used
		);
		_dpf(0, buffer->data, buffer->used);
#endif

		switch (php_http_message_parser_state_pop(parser))
		{
			case PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE:
				return php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE);

			case PHP_HTTP_MESSAGE_PARSER_STATE_START:
			{
				char *ptr = buffer->data;

				while (ptr - buffer->data < buffer->used && PHP_HTTP_IS_CTYPE(space, *ptr)) {
					++ptr;
				}

				php_http_buffer_cut(buffer, 0, ptr - buffer->data);

				if (buffer->used) {
					php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_HEADER);
				}
				break;
			}

			case PHP_HTTP_MESSAGE_PARSER_STATE_HEADER:
			{
				unsigned header_parser_flags = (flags & PHP_HTTP_MESSAGE_PARSER_CLEANUP) ? PHP_HTTP_HEADER_PARSER_CLEANUP : 0;

				switch (php_http_header_parser_parse(&parser->header, buffer, header_parser_flags, *message ? &(*message)->hdrs : NULL, (php_http_info_callback_t) php_http_message_info_callback, message)) {
					case PHP_HTTP_HEADER_PARSER_STATE_FAILURE:
						return PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE;

					case PHP_HTTP_HEADER_PARSER_STATE_DONE:
						php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_HEADER_DONE);
						break;

					default:
						php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_HEADER);
						if (buffer->used) {
							return PHP_HTTP_MESSAGE_PARSER_STATE_HEADER;
						}
				}
				break;
			}

			case PHP_HTTP_MESSAGE_PARSER_STATE_HEADER_DONE:
			{
				zval h, *h_loc = NULL, *h_con = NULL, *h_cl, *h_cr, *h_te, *h_ce;

				if ((h_te = php_http_message_header(*message, ZEND_STRL("Transfer-Encoding")))) {
					Z_TRY_ADDREF_P(h_te);
					zend_hash_str_update(&(*message)->hdrs, "X-Original-Transfer-Encoding", lenof("X-Original-Transfer-Encoding"), h_te);
					zend_hash_str_del(&(*message)->hdrs, "Transfer-Encoding", lenof("Transfer-Encoding"));
				}
				if ((h_cl = php_http_message_header(*message, ZEND_STRL("Content-Length")))) {
					Z_TRY_ADDREF_P(h_cl);
					h_cl = zend_hash_str_update(&(*message)->hdrs, "X-Original-Content-Length", lenof("X-Original-Content-Length"), h_cl);
				}
				if ((h_cr = php_http_message_header(*message, ZEND_STRL("Content-Range")))) {
					Z_TRY_ADDREF_P(h_cr);
					zend_hash_str_update(&(*message)->hdrs, "X-Original-Content-Range", sizeof("X-Original-Content-Range"), h_cr);
					zend_hash_str_del(&(*message)->hdrs, "Content-Range", lenof("Content-Range"));
				}

				/* default */
				ZVAL_LONG(&h, 0);
				zend_hash_str_update(&(*message)->hdrs, "Content-Length", lenof("Content-Length"), &h);

				/* so, if curl sees a 3xx code, a Location header and a Connection:close header
				 * it decides not to read the response body.
				 */
				if ((flags & PHP_HTTP_MESSAGE_PARSER_EMPTY_REDIRECTS)
				&&	(*message)->type == PHP_HTTP_RESPONSE
				&&	(*message)->http.info.response.code/100 == 3
				&&	(h_loc = php_http_message_header(*message, ZEND_STRL("Location")))
				&&	(h_con = php_http_message_header(*message, ZEND_STRL("Connection")))
				) {
					if (php_http_match(Z_STRVAL_P(h_con), "close", PHP_HTTP_MATCH_WORD)) {
						php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_DONE);
						zval_ptr_dtor(h_loc);
						zval_ptr_dtor(h_con);
						break;
					}
				}
				if (h_loc) {
					zval_ptr_dtor(h_loc);
				}
				if (h_con) {
					zval_ptr_dtor(h_con);
				}

				if ((h_ce = php_http_message_header(*message, ZEND_STRL("Content-Encoding")))) {
					if (php_http_match(Z_STRVAL_P(h_ce), "gzip", PHP_HTTP_MATCH_WORD)
					||	php_http_match(Z_STRVAL_P(h_ce), "x-gzip", PHP_HTTP_MATCH_WORD)
					||	php_http_match(Z_STRVAL_P(h_ce), "deflate", PHP_HTTP_MATCH_WORD)
					) {
						if (parser->inflate) {
							php_http_encoding_stream_reset(&parser->inflate);
						} else {
							parser->inflate = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_inflate_ops(), 0);
						}
						Z_TRY_ADDREF_P(h_ce);
						zend_hash_str_update(&(*message)->hdrs, "X-Original-Content-Encoding", lenof("X-Original-Content-Encoding"), h_ce);
						zend_hash_str_del(&(*message)->hdrs, "Content-Encoding", lenof("Content-Encoding"));
					}
				}

				if ((flags & PHP_HTTP_MESSAGE_PARSER_DUMB_BODIES)) {
					php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DUMB);
				} else {
					if (h_te) {
						if (strstr(Z_STRVAL_P(h_te), "chunked")) {
							parser->dechunk = php_http_encoding_stream_init(parser->dechunk, php_http_encoding_stream_get_dechunk_ops(), 0);
							php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_CHUNKED);
							break;
						}
					}

					if (h_cl) {
						char *stop;

						if (Z_TYPE_P(h_cl) == IS_STRING) {
							parser->body_length = strtoul(Z_STRVAL_P(h_cl), &stop, 10);

							if (stop != Z_STRVAL_P(h_cl)) {
								php_http_message_parser_state_push(parser, 1, !parser->body_length?PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE:PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH);
								break;
							}
						} else if (Z_TYPE_P(h_cl) == IS_LONG) {
							parser->body_length = Z_LVAL_P(h_cl);
							php_http_message_parser_state_push(parser, 1, !parser->body_length?PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE:PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH);
							break;
						}
					}

					if (h_cr) {
						ulong total = 0, start = 0, end = 0;

						if (!strncasecmp(Z_STRVAL_P(h_cr), "bytes", lenof("bytes"))
						&& (	Z_STRVAL_P(h_cr)[lenof("bytes")] == ':'
							||	Z_STRVAL_P(h_cr)[lenof("bytes")] == ' '
							||	Z_STRVAL_P(h_cr)[lenof("bytes")] == '='
							)
						) {
							char *total_at = NULL, *end_at = NULL;
							char *start_at = Z_STRVAL_P(h_cr) + sizeof("bytes");

							start = strtoul(start_at, &end_at, 10);
							if (end_at) {
								end = strtoul(end_at + 1, &total_at, 10);
								if (total_at && strncmp(total_at + 1, "*", 1)) {
									total = strtoul(total_at + 1, NULL, 10);
								}

								if (end >= start && (!total || end < total)) {
									parser->body_length = end + 1 - start;
									php_http_message_parser_state_push(parser, 1, !parser->body_length?PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE:PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH);
									break;
								}
							}
						}
					}


					if ((*message)->type == PHP_HTTP_REQUEST) {
						php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_DONE);
					} else {
						php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DUMB);
					}
				}
				break;
			}

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY:
			{
				if (len) {
					zval zcl;

					if (parser->inflate) {
						char *dec_str = NULL;
						size_t dec_len;

						if (SUCCESS != php_http_encoding_stream_update(parser->inflate, str, len, &dec_str, &dec_len)) {
							return php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE);
						}

						if (str != buffer->data) {
							PTR_FREE(str);
						}
						str = dec_str;
						len = dec_len;
					}

					php_stream_write(php_http_message_body_stream((*message)->body), str, len);

					/* keep track */
					ZVAL_LONG(&zcl, php_http_message_body_size((*message)->body));
					zend_hash_str_update(&(*message)->hdrs, "Content-Length", lenof("Content-Length"), &zcl);
				}

				if (cut) {
					php_http_buffer_cut(buffer, 0, cut);
				}

				if (str != buffer->data) {
					PTR_FREE(str);
				}

				str = NULL;
				len = 0;
				cut = 0;
				break;
			}

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DUMB:
			{
				str = buffer->data;
				len = buffer->used;
				cut = len;

				php_http_message_parser_state_push(parser, 2, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE, PHP_HTTP_MESSAGE_PARSER_STATE_BODY);
				break;
			}

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH:
			{
				len = MIN(parser->body_length, buffer->used);
				str = buffer->data;
				cut = len;

				parser->body_length -= len;

				php_http_message_parser_state_push(parser, 2, !parser->body_length?PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE:PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH, PHP_HTTP_MESSAGE_PARSER_STATE_BODY);
				break;
			}

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_CHUNKED:
			{
				/*
				 * - pass available data through the dechunk stream
				 * - pass decoded data along
				 * - if stream zeroed:
				 * 	Y:	- cut processed string out of buffer, but leave length of unprocessed dechunk stream data untouched
				 * 		- body done
				 * 	N:	- parse ahaed
				 */
				char *dec_str = NULL;
				size_t dec_len;

				if (SUCCESS != php_http_encoding_stream_update(parser->dechunk, buffer->data, buffer->used, &dec_str, &dec_len)) {
					return PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE;
				}

				str = dec_str;
				len = dec_len;

				if (php_http_encoding_stream_done(parser->dechunk)) {
					cut = buffer->used - PHP_HTTP_BUFFER(parser->dechunk->ctx)->used;
					php_http_message_parser_state_push(parser, 2, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE, PHP_HTTP_MESSAGE_PARSER_STATE_BODY);
				} else {
					cut = buffer->used;
					php_http_message_parser_state_push(parser, 2, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_CHUNKED, PHP_HTTP_MESSAGE_PARSER_STATE_BODY);
				}
				break;
			}

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE:
			{
				php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_DONE);

				if (parser->dechunk) {
					char *dec_str = NULL;
					size_t dec_len;

					if (SUCCESS != php_http_encoding_stream_finish(parser->dechunk, &dec_str, &dec_len)) {
						return php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE);
					}
					php_http_encoding_stream_dtor(parser->dechunk);

					if (dec_str && dec_len) {
						str = dec_str;
						len = dec_len;
						cut = 0;
						php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_BODY);
					}
				}

				break;
			}

			case PHP_HTTP_MESSAGE_PARSER_STATE_DONE: {
				char *ptr = buffer->data;

				while (ptr - buffer->data < buffer->used && PHP_HTTP_IS_CTYPE(space, *ptr)) {
					++ptr;
				}

				php_http_buffer_cut(buffer, 0, ptr - buffer->data);
				
				if (!(flags & PHP_HTTP_MESSAGE_PARSER_GREEDY)) {
					return PHP_HTTP_MESSAGE_PARSER_STATE_DONE;
				}
				break;
			}
		}
	}

	return php_http_message_parser_state_is(parser);
}

zend_class_entry *php_http_message_parser_class_entry;
static zend_object_handlers php_http_message_parser_object_handlers;

zend_object *php_http_message_parser_object_new(zend_class_entry *ce)
{
	return &php_http_message_parser_object_new_ex(ce, NULL)->zo;
}

php_http_message_parser_object_t *php_http_message_parser_object_new_ex(zend_class_entry *ce, php_http_message_parser_t *parser)
{
	php_http_message_parser_object_t *o;

	o = ecalloc(1, sizeof(php_http_message_parser_object_t) + (ce->default_properties_count - 1) * sizeof(zval));
	zend_object_std_init(&o->zo, ce);
	object_properties_init(&o->zo, ce);

	if (parser) {
		o->parser = parser;
	} else {
		o->parser = php_http_message_parser_init(NULL);
	}
	php_http_buffer_init(&o->buffer);
	o->zo.handlers = &php_http_message_parser_object_handlers;

	return o;
}

void php_http_message_parser_object_free(zend_object *object)
{
	php_http_message_parser_object_t *o = PHP_HTTP_OBJ(object, NULL);

	if (o->parser) {
		php_http_message_parser_free(&o->parser);
	}
	php_http_buffer_dtor(&o->buffer);
	zend_object_std_dtor(object);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageParser_getState, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessageParser, getState)
{
	php_http_message_parser_object_t *parser_obj = PHP_HTTP_OBJ(NULL, getThis());

	zend_parse_parameters_none();
	/* always return the real state */
	RETVAL_LONG(php_http_message_parser_state_is(parser_obj->parser));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageParser_parse, 0, 0, 3)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, flags)
	ZEND_ARG_INFO(1, message)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessageParser, parse)
{
	php_http_message_parser_object_t *parser_obj;
	zval *zmsg;
	char *data_str;
	size_t data_len;
	zend_long flags;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slz", &data_str, &data_len, &flags, &zmsg), invalid_arg, return);

	parser_obj = PHP_HTTP_OBJ(NULL, getThis());
	php_http_buffer_append(&parser_obj->buffer, data_str, data_len);
	RETVAL_LONG(php_http_message_parser_parse(parser_obj->parser, &parser_obj->buffer, flags, &parser_obj->parser->message));

	ZVAL_DEREF(zmsg);
	zval_dtor(zmsg);
	ZVAL_NULL(zmsg);
	if (parser_obj->parser->message) {
		php_http_message_t *msg_cpy = php_http_message_copy(parser_obj->parser->message, NULL);
		php_http_message_object_t *msg_obj = php_http_message_object_new_ex(php_http_message_class_entry, msg_cpy);
		ZVAL_OBJ(zmsg, &msg_obj->zo);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageParser_stream, 0, 0, 3)
	ZEND_ARG_INFO(0, stream)
	ZEND_ARG_INFO(0, flags)
	ZEND_ARG_INFO(1, message)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessageParser, stream)
{
	php_http_message_parser_object_t *parser_obj;
	zend_error_handling zeh;
	zval *zmsg, *zstream;
	php_stream *s;
	zend_long flags;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rlz", &zstream, &flags, &zmsg), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_exception_unexpected_val_class_entry, &zeh);
	php_stream_from_zval(s, zstream);
	zend_restore_error_handling(&zeh);

	parser_obj = PHP_HTTP_OBJ(NULL, getThis());
	RETVAL_LONG(php_http_message_parser_parse_stream(parser_obj->parser, &parser_obj->buffer, s, flags, &parser_obj->parser->message));

	ZVAL_DEREF(zmsg);
	zval_dtor(zmsg);
	ZVAL_NULL(zmsg);
	if (parser_obj->parser->message) {
		php_http_message_t *msg_cpy = php_http_message_copy(parser_obj->parser->message, NULL);
		php_http_message_object_t *msg_obj = php_http_message_object_new_ex(php_http_message_class_entry, msg_cpy);
		ZVAL_OBJ(zmsg, &msg_obj->zo);
	}
}

static zend_function_entry php_http_message_parser_methods[] = {
		PHP_ME(HttpMessageParser, getState, ai_HttpMessageParser_getState, ZEND_ACC_PUBLIC)
		PHP_ME(HttpMessageParser, parse, ai_HttpMessageParser_parse, ZEND_ACC_PUBLIC)
		PHP_ME(HttpMessageParser, stream, ai_HttpMessageParser_stream, ZEND_ACC_PUBLIC)
		{NULL, NULL, NULL}
};

PHP_MINIT_FUNCTION(http_message_parser)
{
	zend_class_entry ce;

	INIT_NS_CLASS_ENTRY(ce, "http\\Message", "Parser", php_http_message_parser_methods);
	php_http_message_parser_class_entry = zend_register_internal_class(&ce);
	memcpy(&php_http_message_parser_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_message_parser_class_entry->create_object = php_http_message_parser_object_new;
	php_http_message_parser_object_handlers.clone_obj = NULL;
	php_http_message_parser_object_handlers.free_obj = php_http_message_parser_object_free;
	php_http_message_parser_object_handlers.offset = XtOffsetOf(php_http_message_parser_object_t, zo);

	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("CLEANUP"), PHP_HTTP_MESSAGE_PARSER_CLEANUP);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("DUMB_BODIES"), PHP_HTTP_MESSAGE_PARSER_DUMB_BODIES);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("EMPTY_REDIRECTS"), PHP_HTTP_MESSAGE_PARSER_EMPTY_REDIRECTS);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("GREEDY"), PHP_HTTP_MESSAGE_PARSER_GREEDY);

	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_FAILURE"), PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_START"), PHP_HTTP_MESSAGE_PARSER_STATE_START);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_HEADER"), PHP_HTTP_MESSAGE_PARSER_STATE_HEADER);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_HEADER_DONE"), PHP_HTTP_MESSAGE_PARSER_STATE_HEADER_DONE);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_BODY"), PHP_HTTP_MESSAGE_PARSER_STATE_BODY);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_BODY_DUMB"), PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DUMB);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_BODY_LENGTH"), PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_BODY_CHUNKED"), PHP_HTTP_MESSAGE_PARSER_STATE_BODY_CHUNKED);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_BODY_DONE"), PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE);
	zend_declare_class_constant_long(php_http_message_parser_class_entry, ZEND_STRL("STATE_DONE"), PHP_HTTP_MESSAGE_PARSER_STATE_DONE);

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

