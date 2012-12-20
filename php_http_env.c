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
#include "php_variables.h"

PHP_RINIT_FUNCTION(http_env)
{
	PHP_HTTP_G->env.request.time = sapi_get_request_time(TSRMLS_C);

	/* populate form data on non-POST requests */
	if (SG(request_info).request_method && strcasecmp(SG(request_info).request_method, "POST") && SG(request_info).content_type && *SG(request_info).content_type) {
		uint ct_len = strlen(SG(request_info).content_type);
		char *ct_str = estrndup(SG(request_info).content_type, ct_len);
		php_http_params_opts_t opts;
		HashTable params;

		php_http_params_opts_default_get(&opts);
		opts.input.str = ct_str;
		opts.input.len = ct_len;

		SG(request_info).content_type_dup = ct_str;

		ZEND_INIT_SYMTABLE(&params);
		if (php_http_params_parse(&params, &opts TSRMLS_CC)) {
			char *key_str;
			uint key_len;
			ulong key_num;

			if (HASH_KEY_IS_STRING == zend_hash_get_current_key_ex(&params, &key_str, &key_len, &key_num, 0, NULL)) {
				sapi_post_entry *post_entry = NULL;

				if (SUCCESS == zend_hash_find(&SG(known_post_content_types), key_str, key_len, (void *) &post_entry)) {
					zval *files = PG(http_globals)[TRACK_VARS_FILES];

					zend_is_auto_global(ZEND_STRL("_POST") TSRMLS_CC);

					if (post_entry) {
						SG(request_info).post_entry = post_entry;

						if (post_entry->post_reader) {
							post_entry->post_reader(TSRMLS_C);
						}
					}

					if (sapi_module.default_post_reader) {
						sapi_module.default_post_reader(TSRMLS_C);
					}

					sapi_handle_post(PG(http_globals)[TRACK_VARS_POST] TSRMLS_CC);

					/*
					 * the rfc1867 handler is an awkward buddy
					 */
					if (files != PG(http_globals)[TRACK_VARS_FILES] && PG(http_globals)[TRACK_VARS_FILES]) {
						Z_ADDREF_P(PG(http_globals)[TRACK_VARS_FILES]);
						zend_hash_update(&EG(symbol_table), "_FILES", sizeof("_FILES"), &PG(http_globals)[TRACK_VARS_FILES], sizeof(zval *), NULL);
						if (files) {
							zval_ptr_dtor(&files);
						}
					}
				}
			}
			zend_hash_destroy(&params);
		}
	}

	STR_SET(SG(request_info).content_type_dup, NULL);

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http_env)
{
	if (PHP_HTTP_G->env.request.headers) {
		zend_hash_destroy(PHP_HTTP_G->env.request.headers);
		FREE_HASHTABLE(PHP_HTTP_G->env.request.headers);
		PHP_HTTP_G->env.request.headers = NULL;
	}
	if (PHP_HTTP_G->env.request.body) {
		php_http_message_body_free(&PHP_HTTP_G->env.request.body);
	}

	if (PHP_HTTP_G->env.server_var) {
		zval_ptr_dtor(&PHP_HTTP_G->env.server_var);
		PHP_HTTP_G->env.server_var = NULL;
	}

	return SUCCESS;
}

PHP_HTTP_API void php_http_env_get_request_headers(HashTable *headers TSRMLS_DC)
{
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	zval **hsv, **header;
	HashPosition pos;

	if (!PHP_HTTP_G->env.request.headers) {
		ALLOC_HASHTABLE(PHP_HTTP_G->env.request.headers);
		zend_hash_init(PHP_HTTP_G->env.request.headers, 0, NULL, ZVAL_PTR_DTOR, 0);

		zend_is_auto_global("_SERVER", lenof("_SERVER") TSRMLS_CC);

		if (SUCCESS == zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void *) &hsv) && Z_TYPE_PP(hsv) == IS_ARRAY) {
			FOREACH_KEY(pos, *hsv, key) {
				if (key.type == HASH_KEY_IS_STRING && key.len > 6 && *key.str == 'H' && !strncmp(key.str, "HTTP_", 5)) {
					key.len -= 5;
					key.str = php_http_pretty_key(estrndup(key.str + 5, key.len - 1), key.len - 1, 1, 1);

					zend_hash_get_current_data_ex(Z_ARRVAL_PP(hsv), (void *) &header, &pos);
					Z_ADDREF_P(*header);
					zend_symtable_update(PHP_HTTP_G->env.request.headers, key.str, key.len, (void *) header, sizeof(zval *), NULL);

					efree(key.str);
				} else if (key.type == HASH_KEY_IS_STRING && key.len > 9 && *key.str == 'C' && !strncmp(key.str, "CONTENT_", 8)) {
					key.str = php_http_pretty_key(estrndup(key.str, key.len - 1), key.len - 1, 1, 1);

					zend_hash_get_current_data_ex(Z_ARRVAL_PP(hsv), (void *) &header, &pos);
					Z_ADDREF_P(*header);
					zend_symtable_update(PHP_HTTP_G->env.request.headers, key.str, key.len, (void *) header, sizeof(zval *), NULL);

					efree(key.str);
				}
			}
		}
	}

	if (headers) {
		zend_hash_copy(headers, PHP_HTTP_G->env.request.headers, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	}
}

PHP_HTTP_API char *php_http_env_get_request_header(const char *name_str, size_t name_len, size_t *len, php_http_message_t *request TSRMLS_DC)
{
	HashTable *request_headers;
	zval **zvalue = NULL;
	char *val = NULL, *key = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);

	if (request) {
		request_headers = &request->hdrs;
	} else {
		php_http_env_get_request_headers(NULL TSRMLS_CC);
		request_headers = PHP_HTTP_G->env.request.headers;
	}

	if (SUCCESS == zend_symtable_find(request_headers, key, name_len + 1, (void *) &zvalue)) {
		zval *zcopy = php_http_ztyp(IS_STRING, *zvalue);

		val = estrndup(Z_STRVAL_P(zcopy), Z_STRLEN_P(zcopy));
		if (len) {
			*len = Z_STRLEN_P(zcopy);
		}
		zval_ptr_dtor(&zcopy);
	}

	efree(key);

	return val;
}

PHP_HTTP_API int php_http_env_got_request_header(const char *name_str, size_t name_len, php_http_message_t *request TSRMLS_DC)
{
	HashTable *request_headers;
	char *key = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);
	int got;

	if (request) {
		request_headers = &request->hdrs;
	} else {
		php_http_env_get_request_headers(NULL TSRMLS_CC);
		request_headers = PHP_HTTP_G->env.request.headers;
	}
	got = zend_symtable_exists(request_headers, key, name_len + 1);
	efree(key);

	return got;
}

PHP_HTTP_API zval *php_http_env_get_superglobal(const char *key, size_t key_len TSRMLS_DC)
{
	zval **hsv;

	zend_is_auto_global(key, key_len TSRMLS_CC);

	if ((SUCCESS != zend_hash_find(&EG(symbol_table), key, key_len + 1, (void *) &hsv)) || (Z_TYPE_PP(hsv) != IS_ARRAY)) {
		return NULL;
	}

	return *hsv;
}

PHP_HTTP_API zval *php_http_env_get_server_var(const char *key, size_t key_len, zend_bool check TSRMLS_DC)
{
	zval *hsv, **var;
	char *env;

	/* if available, this is a lot faster than accessing $_SERVER */
	if (sapi_module.getenv) {
		if ((!(env = sapi_module.getenv((char *) key, key_len TSRMLS_CC))) || (check && !*env)) {
			return NULL;
		}
		if (PHP_HTTP_G->env.server_var) {
			zval_ptr_dtor(&PHP_HTTP_G->env.server_var);
		}
		MAKE_STD_ZVAL(PHP_HTTP_G->env.server_var);
		ZVAL_STRING(PHP_HTTP_G->env.server_var, env, 1);
		return PHP_HTTP_G->env.server_var;
	}

	if (!(hsv = php_http_env_get_superglobal(ZEND_STRL("_SERVER") TSRMLS_CC))) {
		return NULL;
	}
	if ((SUCCESS != zend_symtable_find(Z_ARRVAL_P(hsv), key, key_len + 1, (void *) &var))) {
		return NULL;
	}
	if (check && !((Z_TYPE_PP(var) == IS_STRING) && Z_STRVAL_PP(var) && Z_STRLEN_PP(var))) {
		return NULL;
	}
	return *var;
}

PHP_HTTP_API php_http_message_body_t *php_http_env_get_request_body(TSRMLS_D)
{
	if (!PHP_HTTP_G->env.request.body) {
		php_stream *s = NULL;

		if (SG(request_info).post_data || SG(request_info).raw_post_data) {
			if ((s = php_stream_temp_new())) {
				/* php://input does not support seek() */
				if (SG(request_info).raw_post_data) {
					php_stream_write(s, SG(request_info).raw_post_data, SG(request_info).raw_post_data_length);
				} else {
					php_stream_write(s, SG(request_info).post_data, SG(request_info).post_data_length);
				}
				php_stream_rewind(s);
			}
		} else if (sapi_module.read_post && !SG(read_post_bytes)) {
			if ((s = php_stream_temp_new())) {
				char *buf = emalloc(4096);
				int len;

				while (0 < (len = sapi_module.read_post(buf, 4096 TSRMLS_CC))) {
					SG(read_post_bytes) += len;
					php_stream_write(s, buf, len);

					if (len < 4096) {
						break;
					}
				}
				efree(buf);

				php_stream_rewind(s);
			}
		}
		PHP_HTTP_G->env.request.body = php_http_message_body_init(NULL, s TSRMLS_CC);
	}

	return PHP_HTTP_G->env.request.body;
}

PHP_HTTP_API const char *php_http_env_get_request_method(php_http_message_t *request TSRMLS_DC)
{
	const char *m;

	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, request)) {
		m = request->http.info.request.method;
	} else {
		m = SG(request_info).request_method;
	}

	return m ? m : "GET";
}

PHP_HTTP_API php_http_range_status_t php_http_env_get_request_ranges(HashTable *ranges, size_t length, php_http_message_t *request TSRMLS_DC)
{
	zval *zentry;
	char *range, *rp, c;
	long begin = -1, end = -1, *ptr;

	if (!(range = php_http_env_get_request_header(ZEND_STRL("Range"), NULL, request TSRMLS_CC))) {
		return PHP_HTTP_RANGE_NO;
	}
	if (strncmp(range, "bytes=", lenof("bytes="))) {
		STR_FREE(range);
		return PHP_HTTP_RANGE_NO;
	}

	rp  = range + lenof("bytes=");
	ptr = &begin;

	do {
		switch (c = *(rp++)) {
			case '0':
				/* allow 000... - shall we? */
				if (*ptr != -10) {
					*ptr *= 10;
				}
				break;

			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
				/*
				 * If the value of the pointer is already set (non-negative)
				 * then multiply its value by ten and add the current value,
				 * else initialise the pointers value with the current value
				 * --
				 * This let us recognize empty fields when validating the
				 * ranges, i.e. a "-10" for begin and "12345" for the end
				 * was the following range request: "Range: bytes=0-12345";
				 * While a "-1" for begin and "12345" for the end would
				 * have been: "Range: bytes=-12345".
				 */
				if (*ptr > 0) {
					*ptr *= 10;
					*ptr += c - '0';
				} else {
					*ptr = c - '0';
				}
				break;

			case '-':
				ptr = &end;
				break;

			case ' ':
				break;

			case 0:
			case ',':

				if (length) {
					/* validate ranges */
					switch (begin) {
						/* "0-12345" */
						case -10:
							switch (end) {
								/* "0-" */
								case -1:
									STR_FREE(range);
									return PHP_HTTP_RANGE_NO;

								/* "0-0" */
								case -10:
									end = 0;
									break;

								default:
									if (length <= (size_t) end) {
										end = length - 1;
									}
									break;
							}
							begin = 0;
							break;

						/* "-12345" */
						case -1:
							/* "-", "-0" */
							if (end == -1 || end == -10) {
								STR_FREE(range);
								return PHP_HTTP_RANGE_ERR;
							}
							begin = length - end;
							end = length - 1;
							break;

						/* "12345-(NNN)" */
						default:
							if (length <= (size_t) begin) {
								STR_FREE(range);
								return PHP_HTTP_RANGE_ERR;
							}
							switch (end) {
								/* "12345-0" */
								case -10:
									STR_FREE(range);
									return PHP_HTTP_RANGE_ERR;

								/* "12345-" */
								case -1:
									end = length - 1;
									break;

								/* "12345-67890" */
								default:
									if (length <= (size_t) end) {
										end = length - 1;
									} else if (end <  begin) {
										STR_FREE(range);
										return PHP_HTTP_RANGE_ERR;
									}
									break;
							}
							break;
					}
				}

				MAKE_STD_ZVAL(zentry);
				array_init(zentry);
				add_index_long(zentry, 0, begin);
				add_index_long(zentry, 1, end);
				zend_hash_next_index_insert(ranges, &zentry, sizeof(zval *), NULL);

				begin = -1;
				end = -1;
				ptr = &begin;

				break;

			default:
				STR_FREE(range);
				return PHP_HTTP_RANGE_NO;
		}
	} while (c != 0);

	STR_FREE(range);
	return PHP_HTTP_RANGE_OK;
}

static void grab_headers(void *data, void *arg TSRMLS_DC)
{
	php_http_buffer_appendl(PHP_HTTP_BUFFER(arg), ((sapi_header_struct *)data)->header);
	php_http_buffer_appends(PHP_HTTP_BUFFER(arg), PHP_HTTP_CRLF);
}

PHP_HTTP_API STATUS php_http_env_get_response_headers(HashTable *headers_ht TSRMLS_DC)
{
	STATUS status;
	php_http_buffer_t headers;

	php_http_buffer_init(&headers);
	zend_llist_apply_with_argument(&SG(sapi_headers).headers, grab_headers, &headers TSRMLS_CC);
	php_http_buffer_fix(&headers);

	status = php_http_headers_parse(headers.data, headers.used, headers_ht, NULL, NULL TSRMLS_CC);
	php_http_buffer_dtor(&headers);

	return status;
}

PHP_HTTP_API char *php_http_env_get_response_header(const char *name_str, size_t name_len TSRMLS_DC)
{
	char *val = NULL;
	HashTable headers;

	zend_hash_init(&headers, 0, NULL, ZVAL_PTR_DTOR, 0);
	if (SUCCESS == php_http_env_get_response_headers(&headers TSRMLS_CC)) {
		zval **zvalue;
		char *key = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);

		if (SUCCESS == zend_symtable_find(&headers, key, name_len + 1, (void *) &zvalue)) {
			zval *zcopy = php_http_ztyp(IS_STRING, *zvalue);

			val = estrndup(Z_STRVAL_P(zcopy), Z_STRLEN_P(zcopy));
			zval_ptr_dtor(&zcopy);
		}

		efree(key);
	}
	zend_hash_destroy(&headers);

	return val;
}

PHP_HTTP_API long php_http_env_get_response_code(TSRMLS_D)
{
	long code = SG(sapi_headers).http_response_code;
	return code ? code : 200;
}

PHP_HTTP_API STATUS php_http_env_set_response_code(long http_code TSRMLS_DC)
{
	return sapi_header_op(SAPI_HEADER_SET_STATUS, (void *) http_code TSRMLS_CC);
}

PHP_HTTP_API STATUS php_http_env_set_response_status_line(long code, php_http_version_t *v TSRMLS_DC)
{
	sapi_header_line h = {NULL, 0, 0};
	STATUS ret;

	h.line_len = spprintf(&h.line, 0, "HTTP/%u.%u %ld %s", v->major, v->minor, code, php_http_env_get_response_status_for_code(code));
	ret = sapi_header_op(SAPI_HEADER_REPLACE, (void *) &h TSRMLS_CC);
	efree(h.line);

	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_protocol_version(php_http_version_t *v TSRMLS_DC)
{
	return php_http_env_set_response_status_line(php_http_env_get_response_code(TSRMLS_C), v TSRMLS_CC);
}

PHP_HTTP_API STATUS php_http_env_set_response_header(long http_code, const char *header_str, size_t header_len, zend_bool replace TSRMLS_DC)
{
	sapi_header_line h = {estrndup(header_str, header_len), header_len, http_code};
	STATUS ret = sapi_header_op(replace ? SAPI_HEADER_REPLACE : SAPI_HEADER_ADD, (void *) &h TSRMLS_CC);
	efree(h.line);
	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_header_va(long http_code, zend_bool replace, const char *fmt, va_list argv TSRMLS_DC)
{
	STATUS ret = FAILURE;
	sapi_header_line h = {NULL, 0, http_code};

	h.line_len = vspprintf(&h.line, 0, fmt, argv);

	if (h.line) {
		if (h.line_len) {
			ret = sapi_header_op(replace ? SAPI_HEADER_REPLACE : SAPI_HEADER_ADD, (void *) &h TSRMLS_CC);
		}
		efree(h.line);
	}
	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_header_format(long http_code, zend_bool replace TSRMLS_DC, const char *fmt, ...)
{
	STATUS ret;
	va_list args;

	va_start(args, fmt);
	ret = php_http_env_set_response_header_va(http_code, replace, fmt, args TSRMLS_CC);
	va_end(args);

	return ret;
}

PHP_HTTP_API STATUS php_http_env_set_response_header_value(long http_code, const char *name_str, size_t name_len, zval *value, zend_bool replace TSRMLS_DC)
{
	if (!value) {
		sapi_header_line h = {(char *) name_str, name_len, http_code};

		return sapi_header_op(SAPI_HEADER_DELETE, (void *) &h TSRMLS_CC);
	}

	if(Z_TYPE_P(value) == IS_ARRAY || Z_TYPE_P(value) == IS_OBJECT) {
		HashPosition pos;
		int first = replace;
		zval **data_ptr;

		FOREACH_HASH_VAL(pos, HASH_OF(value), data_ptr) {
			if (SUCCESS != php_http_env_set_response_header_value(http_code, name_str, name_len, *data_ptr, first TSRMLS_CC)) {
				return FAILURE;
			}
			first = 0;
		}

		return SUCCESS;
	} else {
		zval *data = php_http_ztyp(IS_STRING, value);

		if (!Z_STRLEN_P(data)) {
			zval_ptr_dtor(&data);
			return php_http_env_set_response_header_value(http_code, name_str, name_len, NULL, replace TSRMLS_CC);
		} else {
			sapi_header_line h;
			STATUS ret;

			if (name_len > INT_MAX) {
				name_len = INT_MAX;
			}
			h.response_code = http_code;
			h.line_len = spprintf(&h.line, 0, "%.*s: %.*s", (int) name_len, name_str, Z_STRLEN_P(data), Z_STRVAL_P(data));

			ret = sapi_header_op(replace ? SAPI_HEADER_REPLACE : SAPI_HEADER_ADD, (void *) &h TSRMLS_CC);

			zval_ptr_dtor(&data);
			STR_FREE(h.line);

			return ret;
		}
	}
}


static PHP_HTTP_STRLIST(php_http_env_response_status) =
	PHP_HTTP_STRLIST_ITEM("Continue")
	PHP_HTTP_STRLIST_ITEM("Switching Protocols")
	PHP_HTTP_STRLIST_ITEM("Processing")
	PHP_HTTP_STRLIST_NEXT
	PHP_HTTP_STRLIST_ITEM("OK")
	PHP_HTTP_STRLIST_ITEM("Created")
	PHP_HTTP_STRLIST_ITEM("Accepted")
	PHP_HTTP_STRLIST_ITEM("Non-Authoritative Information")
	PHP_HTTP_STRLIST_ITEM("No Content")
	PHP_HTTP_STRLIST_ITEM("Reset Content")
	PHP_HTTP_STRLIST_ITEM("Partial Content")
	PHP_HTTP_STRLIST_ITEM("Multi-Status")
	PHP_HTTP_STRLIST_ITEM("Already Reported")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("IM Used")
	PHP_HTTP_STRLIST_NEXT
	PHP_HTTP_STRLIST_ITEM("Multiple Choices")
	PHP_HTTP_STRLIST_ITEM("Moved Permanently")
	PHP_HTTP_STRLIST_ITEM("Found")
	PHP_HTTP_STRLIST_ITEM("See Other")
	PHP_HTTP_STRLIST_ITEM("Not Modified")
	PHP_HTTP_STRLIST_ITEM("Use Proxy")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("Temporary Redirect")
	PHP_HTTP_STRLIST_ITEM("Permanent Redirect")
	PHP_HTTP_STRLIST_NEXT
	PHP_HTTP_STRLIST_ITEM("Bad Request")
	PHP_HTTP_STRLIST_ITEM("Unauthorized")
	PHP_HTTP_STRLIST_ITEM("Payment Required")
	PHP_HTTP_STRLIST_ITEM("Forbidden")
	PHP_HTTP_STRLIST_ITEM("Not Found")
	PHP_HTTP_STRLIST_ITEM("Method Not Allowed")
	PHP_HTTP_STRLIST_ITEM("Not Acceptable")
	PHP_HTTP_STRLIST_ITEM("Proxy Authentication Required")
	PHP_HTTP_STRLIST_ITEM("Request Timeout")
	PHP_HTTP_STRLIST_ITEM("Conflict")
	PHP_HTTP_STRLIST_ITEM("Gone")
	PHP_HTTP_STRLIST_ITEM("Length Required")
	PHP_HTTP_STRLIST_ITEM("Precondition Failed")
	PHP_HTTP_STRLIST_ITEM("Request Entity Too Large")
	PHP_HTTP_STRLIST_ITEM("Request URI Too Long")
	PHP_HTTP_STRLIST_ITEM("Unsupported Media Type")
	PHP_HTTP_STRLIST_ITEM("Requested Range Not Satisfiable")
	PHP_HTTP_STRLIST_ITEM("Expectation Failed")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("Unprocessible Entity")
	PHP_HTTP_STRLIST_ITEM("Locked")
	PHP_HTTP_STRLIST_ITEM("Failed Dependency")
	PHP_HTTP_STRLIST_ITEM("(Reserved)")
	PHP_HTTP_STRLIST_ITEM("Upgrade Required")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("Precondition Required")
	PHP_HTTP_STRLIST_ITEM("Too Many Requests")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("Request Header Fields Too Large")
	PHP_HTTP_STRLIST_NEXT
	PHP_HTTP_STRLIST_ITEM("Internal Server Error")
	PHP_HTTP_STRLIST_ITEM("Not Implemented")
	PHP_HTTP_STRLIST_ITEM("Bad Gateway")
	PHP_HTTP_STRLIST_ITEM("Service Unavailable")
	PHP_HTTP_STRLIST_ITEM("Gateway Timeout")
	PHP_HTTP_STRLIST_ITEM("HTTP Version Not Supported")
	PHP_HTTP_STRLIST_ITEM("Variant Also Negotiates")
	PHP_HTTP_STRLIST_ITEM("Insufficient Storage")
	PHP_HTTP_STRLIST_ITEM("Loop Detected")
	PHP_HTTP_STRLIST_ITEM("(Unused)")
	PHP_HTTP_STRLIST_ITEM("Not Extended")
	PHP_HTTP_STRLIST_ITEM("Network Authentication Required")
	PHP_HTTP_STRLIST_STOP
;

PHP_HTTP_API const char *php_http_env_get_response_status_for_code(unsigned code)
{
	return php_http_strlist_find(php_http_env_response_status, 100, code);
}


#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpEnv, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpEnv, method, 0)
#define PHP_HTTP_ENV_ME(method)					PHP_ME(HttpEnv, method, PHP_HTTP_ARGS(HttpEnv, method), ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

PHP_HTTP_BEGIN_ARGS(getRequestHeader, 0)
	PHP_HTTP_ARG_VAL(header_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getRequestBody, 0)
	PHP_HTTP_ARG_VAL(body_class_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getResponseStatusForCode, 1)
	PHP_HTTP_ARG_VAL(code, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseStatusForAllCodes);

PHP_HTTP_BEGIN_ARGS(getResponseHeader, 0)
	PHP_HTTP_ARG_VAL(header_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseCode);

PHP_HTTP_BEGIN_ARGS(setResponseHeader, 1)
	PHP_HTTP_ARG_VAL(header_name, 0)
	PHP_HTTP_ARG_VAL(header_value, 0)
	PHP_HTTP_ARG_VAL(response_code, 0)
	PHP_HTTP_ARG_VAL(replace_header, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setResponseCode, 1)
	PHP_HTTP_ARG_VAL(code, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiateLanguage, 1)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(result_array, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiateContentType, 1)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(result_array, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiateCharset, 1)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(result_array, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiateEncoding, 1)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(result_array, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(negotiate, 2)
	PHP_HTTP_ARG_VAL(value, 0)
	PHP_HTTP_ARG_VAL(supported, 0)
	PHP_HTTP_ARG_VAL(primary_type_separator, 0)
	PHP_HTTP_ARG_VAL(result_array, 1)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(statPersistentHandles);

PHP_HTTP_BEGIN_ARGS(cleanPersistentHandles, 0)
	PHP_HTTP_ARG_VAL(name, 0)
	PHP_HTTP_ARG_VAL(ident, 0)
PHP_HTTP_END_ARGS;

static zend_class_entry *php_http_env_class_entry;

zend_class_entry *php_http_env_get_class_entry(void)
{
	return php_http_env_class_entry;
}

static zend_function_entry php_http_env_method_entry[] = {
	PHP_HTTP_ENV_ME(getRequestHeader)
	PHP_HTTP_ENV_ME(getRequestBody)

	PHP_HTTP_ENV_ME(getResponseStatusForCode)
	PHP_HTTP_ENV_ME(getResponseStatusForAllCodes)

	PHP_HTTP_ENV_ME(getResponseHeader)
	PHP_HTTP_ENV_ME(getResponseCode)
	PHP_HTTP_ENV_ME(setResponseHeader)
	PHP_HTTP_ENV_ME(setResponseCode)

	PHP_HTTP_ENV_ME(negotiateLanguage)
	PHP_HTTP_ENV_ME(negotiateContentType)
	PHP_HTTP_ENV_ME(negotiateEncoding)
	PHP_HTTP_ENV_ME(negotiateCharset)
	PHP_HTTP_ENV_ME(negotiate)

	PHP_HTTP_ENV_ME(statPersistentHandles)
	PHP_HTTP_ENV_ME(cleanPersistentHandles)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpEnv, getRequestHeader)
{
	char *header_name_str = NULL;
	int header_name_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!", &header_name_str, &header_name_len)) {
		if (header_name_str && header_name_len) {
			size_t header_length;
			char *header_value = php_http_env_get_request_header(header_name_str, header_name_len, &header_length, NULL TSRMLS_CC);

			if (header_value) {
				RETURN_STRINGL(header_value, header_length, 0);
			}
			RETURN_NULL();
		} else {
			array_init(return_value);
			php_http_env_get_request_headers(Z_ARRVAL_P(return_value) TSRMLS_CC);
			return;
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, getRequestBody)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		zend_class_entry *class_entry = php_http_message_body_get_class_entry();

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|C", &class_entry)) {
			zend_object_value ov;
			php_http_message_body_t *body = php_http_env_get_request_body(TSRMLS_C);

			if (SUCCESS == php_http_new(&ov, class_entry, (php_http_new_t) php_http_message_body_object_new_ex, php_http_message_body_get_class_entry(), php_http_message_body_copy(body, NULL, 0), NULL TSRMLS_CC)) {
				RETVAL_OBJVAL(ov, 0);
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpEnv, getResponseStatusForCode)
{
	long code;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &code)) {
		RETURN_STRING(php_http_env_get_response_status_for_code(code), 1);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, getResponseStatusForAllCodes)
{
	const char *s;
	unsigned c;
	php_http_strlist_iterator_t i;

	if (SUCCESS != zend_parse_parameters_none()) {
		RETURN_FALSE;
	}

	array_init(return_value);
	for (	php_http_strlist_iterator_init(&i, php_http_env_response_status, 100);
			*(s = php_http_strlist_iterator_this(&i, &c));
			php_http_strlist_iterator_next(&i)
	) {
		add_index_string(return_value, c, s, 1);
	}
}

PHP_METHOD(HttpEnv, getResponseHeader)
{
	char *header_name_str = NULL;
	int header_name_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!", &header_name_str, &header_name_len)) {
		if (header_name_str && header_name_len) {
			char *header_value = php_http_env_get_response_header(header_name_str, header_name_len TSRMLS_CC);

			if (header_value) {
				RETURN_STRING(header_value, 0);
			}
			RETURN_NULL();
		} else {
			array_init(return_value);
			php_http_env_get_response_headers(Z_ARRVAL_P(return_value) TSRMLS_CC);
			return;
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, getResponseCode)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_LONG(php_http_env_get_response_code(TSRMLS_C));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, setResponseHeader)
{
	char *header_name_str;
	int header_name_len;
	zval *header_value = NULL;
	long code = 0;
	zend_bool replace_header = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z!lb", &header_name_str, &header_name_len, &header_value, &code, &replace_header)) {
		RETURN_SUCCESS(php_http_env_set_response_header_value(code, header_name_str, header_name_len, header_value, replace_header TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, setResponseCode)
{
	long code;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &code)) {
		RETURN_SUCCESS(php_http_env_set_response_code(code TSRMLS_CC));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, negotiateLanguage)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H|z", &supported, &rs_array)) {
		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}

		PHP_HTTP_DO_NEGOTIATE(language, supported, rs_array);
	} else {
		RETURN_FALSE;
	}
}

PHP_METHOD(HttpEnv, negotiateCharset)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H|z", &supported, &rs_array)) {
		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}
		PHP_HTTP_DO_NEGOTIATE(charset, supported, rs_array);
	} else {
		RETURN_FALSE;
	}
}

PHP_METHOD(HttpEnv, negotiateEncoding)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H|z", &supported, &rs_array)) {
		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}
		PHP_HTTP_DO_NEGOTIATE(encoding, supported, rs_array);
	} else {
		RETURN_FALSE;
	}
}

PHP_METHOD(HttpEnv, negotiateContentType)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H|z", &supported, &rs_array)) {
		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}
		PHP_HTTP_DO_NEGOTIATE(content_type, supported, rs_array);
	} else {
		RETURN_FALSE;
	}
}

PHP_METHOD(HttpEnv, negotiate)
{
	HashTable *supported;
	zval *rs_array = NULL;
	char *value_str, *sep_str = NULL;
	int value_len, sep_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sH|s!z", &value_str, &value_len, &supported, &sep_str, &sep_len, &rs_array)) {
		HashTable *rs;

		if (rs_array) {
			zval_dtor(rs_array);
			array_init(rs_array);
		}

		if ((rs = php_http_negotiate(value_str, value_len, supported, sep_str, sep_len TSRMLS_CC))) {
			PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(rs, supported, rs_array);
		} else {
			PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array);
		}
	} else {
		RETURN_FALSE;
	}
}

PHP_METHOD(HttpEnv, statPersistentHandles)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		object_init(return_value);
		if (php_http_persistent_handle_statall(HASH_OF(return_value) TSRMLS_CC)) {
			return;
		}
		zval_dtor(return_value);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpEnv, cleanPersistentHandles)
{
	char *name_str = NULL, *ident_str = NULL;
	int name_len = 0, ident_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!s!", &name_str, &name_len, &ident_str, &ident_len)) {
		php_http_persistent_handle_cleanup(name_str, name_len, ident_str, ident_len TSRMLS_CC);
	}
}

#ifdef PHP_HTTP_HAVE_JSON
#include "ext/json/php_json.h"

static SAPI_POST_HANDLER_FUNC(php_http_json_post_handler)
{
	if (SG(request_info).raw_post_data) {
		php_json_decode(arg, SG(request_info).raw_post_data, SG(request_info).raw_post_data_length, 1, PG(max_input_nesting_level) TSRMLS_CC);
	}
}

#endif

PHP_MINIT_FUNCTION(http_env)
{
#ifdef PHP_HTTP_HAVE_JSON
	sapi_post_entry entry = {NULL, 0, NULL, NULL};

	entry.post_reader = sapi_read_standard_form_data;
	entry.post_handler = php_http_json_post_handler;

	entry.content_type = "text/json";
	entry.content_type_len = lenof("text/json");
	sapi_register_post_entry(&entry TSRMLS_CC);

	entry.content_type = "application/json";
	entry.content_type_len = lenof("application/json");
	sapi_register_post_entry(&entry TSRMLS_CC);
#endif

	PHP_HTTP_REGISTER_CLASS(http, Env, http_env, NULL, 0);

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
