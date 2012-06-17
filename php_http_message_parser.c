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

PHP_HTTP_API php_http_message_parser_t *php_http_message_parser_init(php_http_message_parser_t *parser TSRMLS_DC)
{
	if (!parser) {
		parser = emalloc(sizeof(*parser));
	}
	memset(parser, 0, sizeof(*parser));

	TSRMLS_SET_CTX(parser->ts);

	php_http_header_parser_init(&parser->header TSRMLS_CC);
	zend_stack_init(&parser->stack);

	return parser;
}

PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_state_push(php_http_message_parser_t *parser, unsigned argc, ...)
{
	php_http_message_parser_state_t state;
	va_list va_args;
	unsigned i;

	va_start(va_args, argc);
	for (i = 0; i < argc; ++i) {
		state  = va_arg(va_args, php_http_message_parser_state_t);
		zend_stack_push(&parser->stack, &state, sizeof(state));
	}
	va_end(va_args);

	return state;
}

PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_state_is(php_http_message_parser_t *parser)
{
	php_http_message_parser_state_t *state;

	if (SUCCESS == zend_stack_top(&parser->stack, (void *) &state)) {
		return *state;
	}
	return PHP_HTTP_MESSAGE_PARSER_STATE_START;
}

PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_state_pop(php_http_message_parser_t *parser)
{
	php_http_message_parser_state_t state, *state_ptr;
	if (SUCCESS == zend_stack_top(&parser->stack, (void *) &state_ptr)) {
		state = *state_ptr;
		zend_stack_del_top(&parser->stack);
		return state;
	}
	return PHP_HTTP_MESSAGE_PARSER_STATE_START;
}

PHP_HTTP_API void php_http_message_parser_dtor(php_http_message_parser_t *parser)
{
	php_http_header_parser_dtor(&parser->header);
	zend_stack_destroy(&parser->stack);
	if (parser->dechunk) {
		php_http_encoding_stream_free(&parser->dechunk);
	}
	if (parser->inflate) {
		php_http_encoding_stream_free(&parser->inflate);
	}
}

PHP_HTTP_API void php_http_message_parser_free(php_http_message_parser_t **parser)
{
	if (*parser) {
		php_http_message_parser_dtor(*parser);
		efree(*parser);
		*parser = NULL;
	}
}

PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_parse_stream(php_http_message_parser_t *parser, php_stream *s, php_http_message_t **message)
{
	php_http_buffer_t buf;
	TSRMLS_FETCH_FROM_CTX(parser->ts);

	php_http_buffer_init_ex(&buf, 0x1000, PHP_HTTP_BUFFER_INIT_PREALLOC);

	while (!php_stream_eof(s)) {
		size_t len = 0;

		switch (php_http_message_parser_state_is(parser)) {
			case PHP_HTTP_MESSAGE_PARSER_STATE_START:
			case PHP_HTTP_MESSAGE_PARSER_STATE_HEADER:
			case PHP_HTTP_MESSAGE_PARSER_STATE_HEADER_DONE:
				/* read line */
				php_stream_get_line(s, buf.data + buf.used, buf.free, &len);
				php_http_buffer_account(&buf, len);
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DUMB:
				/* read all */
				php_http_buffer_account(&buf, php_stream_read(s, buf.data + buf.used, buf.free));
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH:
				/* read body_length */
				php_http_buffer_account(&buf, php_stream_read(s, buf.data + buf.used, MIN(buf.free, parser->body_length)));
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_CHUNKED:
				/* duh, this is very naive */
				if (len) {
					size_t read = php_stream_read(s, buf.data + buf.used, MIN(len, buf.free));

					php_http_buffer_account(&buf, read);

					len -= read;
				} else {
					php_stream_get_line(s, buf.data, buf.free, &len);
					php_http_buffer_account(&buf, len);

					len = strtoul(buf.data - len, NULL, 16);
				}
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY:
			case PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DONE:
				/* should not occur */
				abort();
				break;

			case PHP_HTTP_MESSAGE_PARSER_STATE_DONE:
			case PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE:
				php_http_buffer_dtor(&buf);
				return php_http_message_parser_state_is(parser);
		}

		php_http_message_parser_parse(parser, &buf, 0, message);
	}

	php_http_buffer_dtor(&buf);
	return PHP_HTTP_MESSAGE_PARSER_STATE_DONE;
}


PHP_HTTP_API php_http_message_parser_state_t php_http_message_parser_parse(php_http_message_parser_t *parser, php_http_buffer_t *buffer, unsigned flags, php_http_message_t **message)
{
	char *str = NULL;
	size_t len = 0;
	size_t cut = 0;
	TSRMLS_FETCH_FROM_CTX(parser->ts);

	while (buffer->used || !php_http_message_parser_states[php_http_message_parser_state_is(parser)].need_data) {
#if 0
		const char *state[] = {"START", "HEADER", "HEADER_DONE", "BODY", "BODY_DUMB", "BODY_LENGTH", "BODY_CHUNK", "BODY_DONE", "DONE"};
		fprintf(stderr, "#MP: %s (%d)\n", php_http_message_parser_state_is(parser) < 0 ? "FAILURE" : state[php_http_message_parser_state_is(parser)], message && *message ? (*message)->type : -1);
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
				zval *h, *h_loc = NULL, *h_con = NULL, **h_cl = NULL, **h_cr = NULL, **h_te = NULL;

				if ((h = php_http_message_header(*message, ZEND_STRL("Transfer-Encoding"), 1))) {
					zend_hash_update(&(*message)->hdrs, "X-Original-Transfer-Encoding", sizeof("X-Original-Transfer-Encoding"), &h, sizeof(zval *), (void *) &h_te);
					zend_hash_del(&(*message)->hdrs, "Transfer-Encoding", sizeof("Transfer-Encoding"));
				}
				if ((h = php_http_message_header(*message, ZEND_STRL("Content-Length"), 1))) {
					zend_hash_update(&(*message)->hdrs, "X-Original-Content-Length", sizeof("X-Original-Content-Length"), &h, sizeof(zval *), (void *) &h_cl);
				}
				if ((h = php_http_message_header(*message, ZEND_STRL("Content-Range"), 1))) {
					zend_hash_update(&(*message)->hdrs, "X-Original-Content-Range", sizeof("X-Original-Content-Range"), &h, sizeof(zval *), (void *) &h_cr);
					zend_hash_del(&(*message)->hdrs, "Content-Range", sizeof("Content-Range"));
				}

				/* default */
				MAKE_STD_ZVAL(h);
				ZVAL_LONG(h, 0);
				zend_hash_update(&(*message)->hdrs, "Content-Length", sizeof("Content-Length"), &h, sizeof(zval *), NULL);

				/* so, if curl sees a 3xx code, a Location header and a Connection:close header
				 * it decides not to read the response body.
				 */
				if ((flags & PHP_HTTP_MESSAGE_PARSER_EMPTY_REDIRECTS)
				&&	(*message)->type == PHP_HTTP_RESPONSE
				&&	(*message)->http.info.response.code/100 == 3
				&&	(h_loc = php_http_message_header(*message, ZEND_STRL("Location"), 1))
				&&	(h_con = php_http_message_header(*message, ZEND_STRL("Connection"), 1))
				) {
					if (php_http_match(Z_STRVAL_P(h_con), "close", PHP_HTTP_MATCH_WORD)) {
						php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_DONE);
						zval_ptr_dtor(&h_loc);
						zval_ptr_dtor(&h_con);
						break;
					}
				}
				if (h_loc) {
					zval_ptr_dtor(&h_loc);
				}
				if (h_con) {
					zval_ptr_dtor(&h_con);
				}

				if ((h = php_http_message_header(*message, ZEND_STRL("Content-Encoding"), 1))) {
					if (php_http_match(Z_STRVAL_P(h), "gzip", PHP_HTTP_MATCH_WORD)
					||	php_http_match(Z_STRVAL_P(h), "x-gzip", PHP_HTTP_MATCH_WORD)
					||	php_http_match(Z_STRVAL_P(h), "deflate", PHP_HTTP_MATCH_WORD)
					) {
						if (parser->inflate) {
							php_http_encoding_stream_reset(&parser->inflate);
						} else {
							parser->inflate = php_http_encoding_stream_init(NULL, php_http_encoding_stream_get_inflate_ops(), 0 TSRMLS_CC);
						}
						zend_hash_update(&(*message)->hdrs, "X-Original-Content-Encoding", sizeof("X-Original-Content-Encoding"), &h, sizeof(zval *), NULL);
						zend_hash_del(&(*message)->hdrs, "Content-Encoding", sizeof("Content-Encoding"));
					} else {
						zval_ptr_dtor(&h);
					}
				}

				if ((flags & PHP_HTTP_MESSAGE_PARSER_DUMB_BODIES)) {
					php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_DUMB);
				} else {
					if (h_te) {
						if (strstr(Z_STRVAL_PP(h_te), "chunked")) {
							parser->dechunk = php_http_encoding_stream_init(parser->dechunk, php_http_encoding_stream_get_dechunk_ops(), 0 TSRMLS_CC);
							php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_CHUNKED);
							break;
						}
					}

					if (h_cl) {
						char *stop;

						parser->body_length = strtoul(Z_STRVAL_PP(h_cl), &stop, 10);

						if (stop != Z_STRVAL_PP(h_cl)) {
							php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH);
							break;
						}
					}

					if (h_cr) {
						ulong total = 0, start = 0, end = 0;

						if (!strncasecmp(Z_STRVAL_PP(h_cr), "bytes", lenof("bytes"))
						&& (	Z_STRVAL_P(h)[lenof("bytes")] == ':'
							||	Z_STRVAL_P(h)[lenof("bytes")] == ' '
							||	Z_STRVAL_P(h)[lenof("bytes")] == '='
							)
						) {
							char *total_at = NULL, *end_at = NULL;
							char *start_at = Z_STRVAL_PP(h_cr) + sizeof("bytes");

							start = strtoul(start_at, &end_at, 10);
							if (end_at) {
								end = strtoul(end_at + 1, &total_at, 10);
								if (total_at && strncmp(total_at + 1, "*", 1)) {
									total = strtoul(total_at + 1, NULL, 10);
								}

								if (end >= start && (!total || end < total)) {
									parser->body_length = end + 1 - start;
									php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_BODY_LENGTH);
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
					zval *zcl;

					if (parser->inflate) {
						char *dec_str = NULL;
						size_t dec_len;

						if (SUCCESS != php_http_encoding_stream_update(parser->inflate, str, len, &dec_str, &dec_len)) {
							return php_http_message_parser_state_push(parser, 1, PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE);
						}

						if (str != buffer->data) {
							STR_FREE(str);
						}
						str = dec_str;
						len = dec_len;
					}

					php_stream_write(php_http_message_body_stream(&(*message)->body), str, len);

					/* keep track */
					MAKE_STD_ZVAL(zcl);
					ZVAL_LONG(zcl, php_http_message_body_size(&(*message)->body));
					zend_hash_update(&(*message)->hdrs, "Content-Length", sizeof("Content-Length"), &zcl, sizeof(zval *), NULL);
				}

				if (cut) {
					php_http_buffer_cut(buffer, 0, cut);
				}

				if (str != buffer->data) {
					STR_FREE(str);
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
					return FAILURE;
				}

				str = dec_str;
				len = dec_len;

				if (php_http_encoding_stream_done(parser->dechunk)) {
					cut = buffer->used - PHP_HTTP_BUFFER_LEN(parser->dechunk->ctx);
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
				break;
			}
		}
	}

	return php_http_message_parser_state_is(parser);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

