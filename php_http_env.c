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
#include "php_variables.h"

PHP_RINIT_FUNCTION(http_env)
{
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
		if (php_http_params_parse(&params, &opts)) {
			zend_string *key_str;
			zend_ulong key_num;

			if (HASH_KEY_IS_STRING == zend_hash_get_current_key(&params, &key_str, &key_num)) {
				sapi_post_entry *post_entry = NULL;

				if ((post_entry = zend_hash_find_ptr(&SG(known_post_content_types), key_str))) {
					SG(request_info).post_entry = post_entry;

					if (post_entry->post_reader) {
						post_entry->post_reader();
					}

					if (sapi_module.default_post_reader) {
						sapi_module.default_post_reader();
					}

					sapi_handle_post(&PG(http_globals)[TRACK_VARS_POST]);

					/*
					 * the rfc1867 handler is an awkward buddy
					 * FIXME: this leaks because php_auto_globals_create_files()
					 * as well as the rfc1867_handler call
					 * array_init(&PG(http_globals)[TRACK_VARS_FILES])
					 */
					Z_TRY_ADDREF(PG(http_globals)[TRACK_VARS_FILES]);
					zend_hash_str_update(&EG(symbol_table).ht, "_FILES", lenof("_FILES"), &PG(http_globals)[TRACK_VARS_FILES]);
				}
			}
			zend_hash_destroy(&params);
		}
	}

	PTR_SET(SG(request_info).content_type_dup, NULL);

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
		zval_ptr_dtor(PHP_HTTP_G->env.server_var);
		PHP_HTTP_G->env.server_var = NULL;
	}

	return SUCCESS;
}

void php_http_env_get_request_headers(HashTable *headers TSRMLS_DC)
{
	php_http_arrkey_t key;
	zval *hsv, *header;

	if (!PHP_HTTP_G->env.request.headers) {
		ALLOC_HASHTABLE(PHP_HTTP_G->env.request.headers);
		ZEND_INIT_SYMTABLE(PHP_HTTP_G->env.request.headers);

		if ((hsv = php_http_env_get_superglobal(ZEND_STRL("_SERVER")))) {
			ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(hsv), key.h, key.key, header)
			{
				if (key.key && key.key->len > 5 && *key.key->val == 'H' && !strncmp(key.key->val, "HTTP_", 5)) {
					size_t key_len = key.key->len - 5;
					char *key_str = php_http_pretty_key(estrndup(&key.key->val[5], key_len), key_len, 1, 1);

					Z_TRY_ADDREF_P(header);
					zend_symtable_str_update(PHP_HTTP_G->env.request.headers, key_str, key_len, header);

					efree(key_str);
				} else if (key.key && key.key->len > 8 && *key.key->val == 'C' && !strncmp(key.key->val, "CONTENT_", 8)) {
					char *key_str = php_http_pretty_key(estrndup(key.key->val, key.key->len), key.key->len, 1, 1);

					Z_TRY_ADDREF_P(header);
					zend_symtable_str_update(PHP_HTTP_G->env.request.headers, key_str, key.key->len, header);

					efree(key_str);
				}
			}
			ZEND_HASH_FOREACH_END();
		}
	}

	if (headers) {
		array_copy(PHP_HTTP_G->env.request.headers, headers);
	}
}

char *php_http_env_get_request_header(const char *name_str, size_t name_len, size_t *len, php_http_message_t *request)
{
	HashTable *request_headers;
	zval *zvalue = NULL;
	char *val = NULL, *key = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);

	if (request) {
		request_headers = &request->hdrs;
	} else {
		php_http_env_get_request_headers(NULL);
		request_headers = PHP_HTTP_G->env.request.headers;
	}

	if ((zvalue = zend_symtable_str_find(request_headers, key, name_len))) {
		zend_string *zs = zval_get_string(zvalue);

		val = estrndup(zs->val, zs->len);
		if (len) {
			*len = zs->len;
		}
		zend_string_release(zs);
	}

	efree(key);

	return val;
}

zend_bool php_http_env_got_request_header(const char *name_str, size_t name_len, php_http_message_t *request)
{
	HashTable *request_headers;
	char *key = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);
	zend_bool got;

	if (request) {
		request_headers = &request->hdrs;
	} else {
		php_http_env_get_request_headers(NULL);
		request_headers = PHP_HTTP_G->env.request.headers;
	}
	got = zend_symtable_str_exists(request_headers, key, name_len);
	efree(key);

	return got;
}

zval *php_http_env_get_superglobal(const char *key, size_t key_len)
{
	zval *hsv;
	zend_string *key_str = zend_string_init(key, key_len, 0);

	zend_is_auto_global(key_str);
	hsv = zend_hash_find(&EG(symbol_table).ht, key_str);
	zend_string_release(key_str);

	if (Z_TYPE_P(hsv) != IS_ARRAY) {
		return NULL;
	}

	return hsv;
}

zval *php_http_env_get_server_var(const char *key, size_t key_len, zend_bool check)
{
	zval *hsv, *var;

	/* if available, this is a lot faster than accessing $_SERVER * /
	if (sapi_module.getenv) {
		char *env;

		if ((!(env = sapi_module.getenv((char *) key, key_len))) || (check && !*env)) {
			return NULL;
		}
		if (PHP_HTTP_G->env.server_var) {
			zval_ptr_dtor(&PHP_HTTP_G->env.server_var);
		}
		MAKE_STD_ZVAL(PHP_HTTP_G->env.server_var);
		ZVAL_STRING(PHP_HTTP_G->env.server_var, env, 1);
		return PHP_HTTP_G->env.server_var;
	}
	/ * */

	if (!(hsv = php_http_env_get_superglobal(ZEND_STRL("_SERVER")))) {
		return NULL;
	}
	if (!(var = zend_symtable_str_find(Z_ARRVAL_P(hsv), key, key_len))) {
		return NULL;
	}
	if (check && !((Z_TYPE_P(var) == IS_STRING) && Z_STRVAL_P(var) && Z_STRLEN_P(var))) {
		return NULL;
	}
	return var;
}

php_http_message_body_t *php_http_env_get_request_body(void)
{
	if (!PHP_HTTP_G->env.request.body) {
		php_stream *s = php_stream_temp_new();
		php_stream *input = php_stream_open_wrapper("php://input", "r", 0, NULL);

		/* php://input does not support stat */
		php_stream_copy_to_stream_ex(input, s, -1, NULL);
		php_stream_close(input);

		php_stream_rewind(s);
		PHP_HTTP_G->env.request.body = php_http_message_body_init(NULL, s);
	}

	return PHP_HTTP_G->env.request.body;
}

const char *php_http_env_get_request_method(php_http_message_t *request)
{
	const char *m;

	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, request)) {
		m = request->http.info.request.method;
	} else {
		m = SG(request_info).request_method;
	}

	return m ? m : "GET";
}

php_http_range_status_t php_http_env_get_request_ranges(HashTable *ranges, size_t length, php_http_message_t *request)
{
	zval zentry;
	char *range, *rp, c;
	long begin = -1, end = -1, *ptr;

	if (!(range = php_http_env_get_request_header(ZEND_STRL("Range"), NULL, request))) {
		return PHP_HTTP_RANGE_NO;
	}
	if (strncmp(range, "bytes=", lenof("bytes="))) {
		PTR_FREE(range);
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
									PTR_FREE(range);
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
								PTR_FREE(range);
								return PHP_HTTP_RANGE_ERR;
							}
							begin = length - end;
							end = length - 1;
							break;

						/* "12345-(NNN)" */
						default:
							if (length <= (size_t) begin) {
								PTR_FREE(range);
								return PHP_HTTP_RANGE_ERR;
							}
							switch (end) {
								/* "12345-0" */
								case -10:
									PTR_FREE(range);
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
										PTR_FREE(range);
										return PHP_HTTP_RANGE_ERR;
									}
									break;
							}
							break;
					}
				}

				array_init(&zentry);
				add_index_long(&zentry, 0, begin);
				add_index_long(&zentry, 1, end);
				zend_hash_next_index_insert(ranges, &zentry);

				begin = -1;
				end = -1;
				ptr = &begin;

				break;

			default:
				PTR_FREE(range);
				return PHP_HTTP_RANGE_NO;
		}
	} while (c != 0);

	PTR_FREE(range);
	return PHP_HTTP_RANGE_OK;
}

static void grab_headers(void *data, void *arg)
{
	php_http_buffer_appendl(PHP_HTTP_BUFFER(arg), ((sapi_header_struct *)data)->header);
	php_http_buffer_appends(PHP_HTTP_BUFFER(arg), PHP_HTTP_CRLF);
}

static void grab_header(void *data, void *arg)
{
	struct {
		char *name_str;
		size_t name_len;
		char *value_ptr;
	} *args = arg;
	sapi_header_struct *header = data;

	if (	header->header_len > args->name_len
	&& header->header[args->name_len] == ':'
	&& !strncmp(header->header, args->name_str, args->name_len)
	) {
		args->value_ptr = &header->header[args->name_len + 1];
	}
}

ZEND_RESULT_CODE php_http_env_get_response_headers(HashTable *headers_ht)
{
	ZEND_RESULT_CODE status;
	php_http_buffer_t headers;

	php_http_buffer_init(&headers);
	zend_llist_apply_with_argument(&SG(sapi_headers).headers, grab_headers, &headers);
	php_http_buffer_fix(&headers);

	status = php_http_header_parse(headers.data, headers.used, headers_ht, NULL, NULL);
	php_http_buffer_dtor(&headers);

	return status;
}

char *php_http_env_get_response_header(const char *name_str, size_t name_len)
{
	struct {
		char *name_str;
		size_t name_len;
		char *value_ptr;
	} args;

	args.name_str = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);
	args.name_len = name_len;
	args.value_ptr = NULL;
	zend_llist_apply_with_argument(&SG(sapi_headers).headers, grab_header, &args);
	efree(args.name_str);

	return args.value_ptr ? estrdup(args.value_ptr) : NULL;
}

long php_http_env_get_response_code(void)
{
	long code = SG(sapi_headers).http_response_code;
	return code ? code : 200;
}

ZEND_RESULT_CODE php_http_env_set_response_code(long http_code)
{
	return sapi_header_op(SAPI_HEADER_SET_STATUS, (void *) (zend_intptr_t) http_code);
}

ZEND_RESULT_CODE php_http_env_set_response_status_line(long code, php_http_version_t *v)
{
	sapi_header_line h = {NULL, 0, 0};
	ZEND_RESULT_CODE ret;

	h.line_len = spprintf(&h.line, 0, "HTTP/%u.%u %ld %s", v->major, v->minor, code, php_http_env_get_response_status_for_code(code));
	ret = sapi_header_op(SAPI_HEADER_REPLACE, (void *) &h);
	efree(h.line);

	return ret;
}

ZEND_RESULT_CODE php_http_env_set_response_protocol_version(php_http_version_t *v)
{
	return php_http_env_set_response_status_line(php_http_env_get_response_code(), v);
}

ZEND_RESULT_CODE php_http_env_set_response_header(long http_code, const char *header_str, size_t header_len, zend_bool replace)
{
	sapi_header_line h = {estrndup(header_str, header_len), header_len, http_code};
	ZEND_RESULT_CODE ret = sapi_header_op(replace ? SAPI_HEADER_REPLACE : SAPI_HEADER_ADD, (void *) &h);
	efree(h.line);
	return ret;
}

ZEND_RESULT_CODE php_http_env_set_response_header_va(long http_code, zend_bool replace, const char *fmt, va_list argv)
{
	ZEND_RESULT_CODE ret = FAILURE;
	sapi_header_line h = {NULL, 0, http_code};

	h.line_len = vspprintf(&h.line, 0, fmt, argv);

	if (h.line) {
		if (h.line_len) {
			ret = sapi_header_op(replace ? SAPI_HEADER_REPLACE : SAPI_HEADER_ADD, (void *) &h);
		}
		efree(h.line);
	}
	return ret;
}

ZEND_RESULT_CODE php_http_env_set_response_header_format(long http_code, zend_bool replace, const char *fmt, ...)
{
	ZEND_RESULT_CODE ret;
	va_list args;

	va_start(args, fmt);
	ret = php_http_env_set_response_header_va(http_code, replace, fmt, args);
	va_end(args);

	return ret;
}

ZEND_RESULT_CODE php_http_env_set_response_header_value(long http_code, const char *name_str, size_t name_len, zval *value, zend_bool replace)
{
	if (!value) {
		sapi_header_line h = {(char *) name_str, name_len, http_code};

		return sapi_header_op(SAPI_HEADER_DELETE, (void *) &h);
	}

	if (Z_TYPE_P(value) == IS_ARRAY || Z_TYPE_P(value) == IS_OBJECT) {
		int first = replace;
		zval *data_ptr;
		HashTable *ht = HASH_OF(value);

		ZEND_HASH_FOREACH_VAL(ht, data_ptr)
		{
			if (SUCCESS != php_http_env_set_response_header_value(http_code, name_str, name_len, data_ptr, first)) {
				return FAILURE;
			}
			first = 0;
		}
		ZEND_HASH_FOREACH_END();

		return SUCCESS;
	} else {
		zend_string *data = zval_get_string(value);

		if (!data->len) {
			zend_string_release(data);
			return php_http_env_set_response_header_value(http_code, name_str, name_len, NULL, replace);
		} else {
			sapi_header_line h;
			ZEND_RESULT_CODE ret;

			if (name_len > INT_MAX) {
				return FAILURE;
			}
			h.response_code = http_code;
			h.line_len = spprintf(&h.line, 0, "%.*s: %.*s", (int) name_len, name_str, data->len, data->val);

			ret = sapi_header_op(replace ? SAPI_HEADER_REPLACE : SAPI_HEADER_ADD, (void *) &h);

			zend_string_release(data);
			PTR_FREE(h.line);

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

const char *php_http_env_get_response_status_for_code(unsigned code)
{
	return php_http_strlist_find(php_http_env_response_status, 100, code);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_getRequestHeader, 0, 0, 0)
	ZEND_ARG_INFO(0, header_name)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, getRequestHeader)
{
	char *header_name_str = NULL;
	size_t header_name_len = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "|s!", &header_name_str, &header_name_len)) {
		return;
	}
	if (header_name_str && header_name_len) {
		size_t header_length;
		char *header_value = php_http_env_get_request_header(header_name_str, header_name_len, &header_length, NULL);

		if (header_value) {
			RETURN_STR(php_http_cs2zs(header_value, header_length));
		}
	} else {
		array_init(return_value);
		php_http_env_get_request_headers(Z_ARRVAL_P(return_value));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_getRequestBody, 0, 0, 0)
	ZEND_ARG_INFO(0, body_class_name)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, getRequestBody)
{
	php_http_message_body_t *body;
	php_http_message_body_object_t *body_obj;
	zend_class_entry *class_entry = php_http_message_body_class_entry;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|C", &class_entry), invalid_arg, return);

	body = php_http_env_get_request_body();
	if (SUCCESS == php_http_new((void *) &body_obj, class_entry, (php_http_new_t) php_http_message_body_object_new_ex, php_http_message_body_class_entry, body)) {
		php_http_message_body_addref(body);
		RETVAL_OBJ(&body_obj->zo);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_getResponseStatusForCode, 0, 0, 1)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, getResponseStatusForCode)
{
	zend_long code;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &code)) {
		return;
	}
	RETURN_STRING(php_http_env_get_response_status_for_code(code));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_getResponseStatusForAllCodes, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, getResponseStatusForAllCodes)
{
	const char *s;
	unsigned c;
	php_http_strlist_iterator_t i;

	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}

	array_init(return_value);
	for (	php_http_strlist_iterator_init(&i, php_http_env_response_status, 100);
			*(s = php_http_strlist_iterator_this(&i, &c));
			php_http_strlist_iterator_next(&i)
	) {
		add_index_string(return_value, c, s);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_getResponseHeader, 0, 0, 0)
	ZEND_ARG_INFO(0, header_name)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, getResponseHeader)
{
	char *header_name_str = NULL;
	size_t header_name_len = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "|s!", &header_name_str, &header_name_len)) {
		return;
	}
	if (header_name_str && header_name_len) {
		char *header_value = php_http_env_get_response_header(header_name_str, header_name_len);

		if (header_value) {
			RETURN_STR(php_http_cs2zs(header_value, strlen(header_value)));
		}
	} else {
		array_init(return_value);
		php_http_env_get_response_headers(Z_ARRVAL_P(return_value));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_getResponseCode, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, getResponseCode)
{
	if (SUCCESS != zend_parse_parameters_none()) {
		return;
	}
	RETURN_LONG(php_http_env_get_response_code());
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_setResponseHeader, 0, 0, 1)
	ZEND_ARG_INFO(0, header_name)
	ZEND_ARG_INFO(0, header_value)
	ZEND_ARG_INFO(0, response_code)
	ZEND_ARG_INFO(0, replace_header)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, setResponseHeader)
{
	char *header_name_str;
	int header_name_len;
	zval *header_value = NULL;
	long code = 0;
	zend_bool replace_header = 1;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "s|z!lb", &header_name_str, &header_name_len, &header_value, &code, &replace_header)) {
		return;
	}
	RETURN_BOOL(SUCCESS == php_http_env_set_response_header_value(code, header_name_str, header_name_len, header_value, replace_header TSRMLS_CC));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_setResponseCode, 0, 0, 1)
	ZEND_ARG_INFO(0, code)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, setResponseCode)
{
	zend_long code;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "l", &code)) {
		return;
	}
	RETURN_BOOL(SUCCESS == php_http_env_set_response_code(code));
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_negotiateLanguage, 0, 0, 1)
	ZEND_ARG_INFO(0, supported)
	ZEND_ARG_INFO(1, result_array)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, negotiateLanguage)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "H|z", &supported, &rs_array)) {
		return;
	}
	if (rs_array) {
		ZVAL_DEREF(rs_array);
		zval_dtor(rs_array);
		array_init(rs_array);
	}

	PHP_HTTP_DO_NEGOTIATE(language, supported, rs_array);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_negotiateCharset, 0, 0, 1)
	ZEND_ARG_INFO(0, supported)
	ZEND_ARG_INFO(1, result_array)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, negotiateCharset)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "H|z", &supported, &rs_array)) {
		return;
	}
	if (rs_array) {
		ZVAL_DEREF(rs_array);
		zval_dtor(rs_array);
		array_init(rs_array);
	}
	PHP_HTTP_DO_NEGOTIATE(charset, supported, rs_array);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_negotiateEncoding, 0, 0, 1)
	ZEND_ARG_INFO(0, supported)
	ZEND_ARG_INFO(1, result_array)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, negotiateEncoding)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "H|z", &supported, &rs_array)) {
		return;
	}
	if (rs_array) {
		ZVAL_DEREF(rs_array);
		zval_dtor(rs_array);
		array_init(rs_array);
	}
	PHP_HTTP_DO_NEGOTIATE(encoding, supported, rs_array);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_negotiateContentType, 0, 0, 1)
	ZEND_ARG_INFO(0, supported)
	ZEND_ARG_INFO(1, result_array)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, negotiateContentType)
{
	HashTable *supported;
	zval *rs_array = NULL;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "H|z", &supported, &rs_array)) {
		return;
	}
	if (rs_array) {
		ZVAL_DEREF(rs_array);
		zval_dtor(rs_array);
		array_init(rs_array);
	}
	PHP_HTTP_DO_NEGOTIATE(content_type, supported, rs_array);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpEnv_negotiate, 0, 0, 2)
	ZEND_ARG_INFO(0, params)
	ZEND_ARG_INFO(0, supported)
	ZEND_ARG_INFO(0, primary_type_separator)
	ZEND_ARG_INFO(1, result_array)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpEnv, negotiate)
{
	HashTable *supported, *rs;
	zval *rs_array = NULL;
	char *value_str, *sep_str = NULL;
	size_t value_len, sep_len = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sH|s!z", &value_str, &value_len, &supported, &sep_str, &sep_len, &rs_array)) {
		return;
	}

	if (rs_array) {
		ZVAL_DEREF(rs_array);
		zval_dtor(rs_array);
		array_init(rs_array);
	}

	if ((rs = php_http_negotiate(value_str, value_len, supported, sep_str, sep_len))) {
		PHP_HTTP_DO_NEGOTIATE_HANDLE_RESULT(rs, supported, rs_array);
	} else {
		PHP_HTTP_DO_NEGOTIATE_HANDLE_DEFAULT(supported, rs_array);
	}
}

static zend_function_entry php_http_env_methods[] = {
	PHP_ME(HttpEnv, getRequestHeader,              ai_HttpEnv_getRequestHeader,              ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpEnv, getRequestBody,                ai_HttpEnv_getRequestBody,                ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	PHP_ME(HttpEnv, getResponseStatusForCode,      ai_HttpEnv_getResponseStatusForCode,      ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpEnv, getResponseStatusForAllCodes,  ai_HttpEnv_getResponseStatusForAllCodes,  ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	PHP_ME(HttpEnv, getResponseHeader,             ai_HttpEnv_getResponseHeader,             ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpEnv, getResponseCode,               ai_HttpEnv_getResponseCode,               ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpEnv, setResponseHeader,             ai_HttpEnv_setResponseHeader,             ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpEnv, setResponseCode,               ai_HttpEnv_setResponseCode,               ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	PHP_ME(HttpEnv, negotiateLanguage,             ai_HttpEnv_negotiateLanguage,             ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpEnv, negotiateContentType,          ai_HttpEnv_negotiateContentType,          ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpEnv, negotiateEncoding,             ai_HttpEnv_negotiateEncoding,             ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpEnv, negotiateCharset,              ai_HttpEnv_negotiateCharset,              ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	PHP_ME(HttpEnv, negotiate,                     ai_HttpEnv_negotiate,                     ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	EMPTY_FUNCTION_ENTRY
};

#ifdef PHP_HTTP_HAVE_JSON
#include "ext/json/php_json.h"

static SAPI_POST_HANDLER_FUNC(php_http_json_post_handler)
{
	zval *zarg = arg;
	zend_string *json = NULL;

	if (SG(request_info).request_body) {
		/* FG(stream_wrappers) not initialized yet, so we cannot use php://input */
		php_stream_rewind(SG(request_info).request_body);
		json = php_stream_copy_to_mem(SG(request_info).request_body, PHP_STREAM_COPY_ALL, 0);
	}

	if (json) {
		if (json->len) {
			zval zjson;

			ZVAL_NULL(&zjson);
			php_json_decode(&zjson, json->val, json->len, 1, PG(max_input_nesting_level));
			if (Z_TYPE(zjson) != IS_NULL) {
				zval_dtor(zarg);
				ZVAL_COPY_VALUE(zarg, (&zjson));
			}
		}
		zend_string_release(json);
	}
}

static void php_http_env_register_json_handler(void)
{
	sapi_post_entry entry = {NULL, 0, NULL, NULL};

	entry.post_reader = sapi_read_standard_form_data;
	entry.post_handler = php_http_json_post_handler;

	entry.content_type = "text/json";
	entry.content_type_len = lenof("text/json");
	sapi_register_post_entry(&entry);

	entry.content_type = "application/json";
	entry.content_type_len = lenof("application/json");
	sapi_register_post_entry(&entry);
}
#endif

zend_class_entry *php_http_env_class_entry;

PHP_MINIT_FUNCTION(http_env)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Env", php_http_env_methods);
	php_http_env_class_entry = zend_register_internal_class(&ce);

#ifdef PHP_HTTP_HAVE_JSON
	php_http_env_register_json_handler();
#endif

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
