/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2013, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

static void message_headers(php_http_message_t *msg, php_http_buffer_t *str);

PHP_HTTP_API zend_bool php_http_message_info_callback(php_http_message_t **message, HashTable **headers, php_http_info_t *info TSRMLS_DC)
{
	php_http_message_t *old = *message;

	/* advance message */
	if (!old || old->type || zend_hash_num_elements(&old->hdrs)) {
		(*message) = php_http_message_init(NULL, 0, NULL TSRMLS_CC);
		(*message)->parent = old;
		if (headers) {
			(*headers) = &((*message)->hdrs);
		}
	}

	if (info) {
		php_http_message_set_info(*message, info);
	}

	return old != *message;
}

PHP_HTTP_API php_http_message_t *php_http_message_init(php_http_message_t *message, php_http_message_type_t type, php_http_message_body_t *body TSRMLS_DC)
{
	if (!message) {
		message = emalloc(sizeof(*message));
	}
	memset(message, 0, sizeof(*message));
	TSRMLS_SET_CTX(message->ts);

	php_http_message_set_type(message, type);
	message->http.version.major = 1;
	message->http.version.minor = 1;
	zend_hash_init(&message->hdrs, 0, NULL, ZVAL_PTR_DTOR, 0);
	message->body = body ? body : php_http_message_body_init(NULL, NULL TSRMLS_CC);

	return message;
}

PHP_HTTP_API php_http_message_t *php_http_message_init_env(php_http_message_t *message, php_http_message_type_t type TSRMLS_DC)
{
	int free_msg = !message;
	zval *sval, tval;
	php_http_message_body_t *mbody;
	
	switch (type) {
		case PHP_HTTP_REQUEST:
			mbody = php_http_env_get_request_body(TSRMLS_C);
			php_http_message_body_addref(mbody);
			message = php_http_message_init(message, type, mbody TSRMLS_CC);
			if ((sval = php_http_env_get_server_var(ZEND_STRL("SERVER_PROTOCOL"), 1 TSRMLS_CC)) && !strncmp(Z_STRVAL_P(sval), "HTTP/", lenof("HTTP/"))) {
				php_http_version_parse(&message->http.version, Z_STRVAL_P(sval) TSRMLS_CC);
			}
			if ((sval = php_http_env_get_server_var(ZEND_STRL("REQUEST_METHOD"), 1 TSRMLS_CC))) {
				message->http.info.request.method = estrdup(Z_STRVAL_P(sval));
			}
			if ((sval = php_http_env_get_server_var(ZEND_STRL("REQUEST_URI"), 1 TSRMLS_CC))) {
				message->http.info.request.url = estrdup(Z_STRVAL_P(sval));
			}
			
			php_http_env_get_request_headers(&message->hdrs TSRMLS_CC);
			break;
			
		case PHP_HTTP_RESPONSE:
			message = php_http_message_init(NULL, type, NULL TSRMLS_CC);
			if (!SG(sapi_headers).http_status_line || !php_http_info_parse((php_http_info_t *) &message->http, SG(sapi_headers).http_status_line TSRMLS_CC)) {
				if (!(message->http.info.response.code = SG(sapi_headers).http_response_code)) {
					message->http.info.response.code = 200;
				}
				message->http.info.response.status = estrdup(php_http_env_get_response_status_for_code(message->http.info.response.code));
			}
			
			php_http_env_get_response_headers(&message->hdrs TSRMLS_CC);
#if PHP_VERSION_ID >= 50400
			if (php_output_get_level(TSRMLS_C)) {
				if (php_output_get_status(TSRMLS_C) & PHP_OUTPUT_SENT) {
					php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "Could not fetch response body, output has already been sent at %s:%d", php_output_get_start_filename(TSRMLS_C), php_output_get_start_lineno(TSRMLS_C));
					goto error;
				} else if (SUCCESS != php_output_get_contents(&tval TSRMLS_CC)) {
					php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "Could not fetch response body");
					goto error;
				} else {
					php_http_message_body_append(message->body, Z_STRVAL(tval), Z_STRLEN(tval));
					zval_dtor(&tval);
				}
			}
#endif
			break;
			
		default:
		error:
			if (free_msg) {
				if (message) {
					php_http_message_free(&message);
				}
			} else {
				message = NULL;
			}
			break;
	}
	
	return message;
}

PHP_HTTP_API php_http_message_t *php_http_message_parse(php_http_message_t *msg, const char *str, size_t len, zend_bool greedy TSRMLS_DC)
{
	php_http_message_parser_t p;
	php_http_buffer_t buf;
	unsigned flags = PHP_HTTP_MESSAGE_PARSER_CLEANUP;
	int free_msg;

	php_http_buffer_from_string_ex(&buf, str, len);
	php_http_message_parser_init(&p TSRMLS_CC);

	if ((free_msg = !msg)) {
		msg = php_http_message_init(NULL, 0, NULL TSRMLS_CC);
	}

	if (greedy) {
		flags |= PHP_HTTP_MESSAGE_PARSER_GREEDY;
	}
	if (PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE == php_http_message_parser_parse(&p, &buf, flags, &msg)) {
		if (free_msg) {
			php_http_message_free(&msg);
		}
		msg = NULL;
	}

	php_http_message_parser_dtor(&p);
	php_http_buffer_dtor(&buf);

	return msg;
}

PHP_HTTP_API zval *php_http_message_header(php_http_message_t *msg, const char *key_str, size_t key_len, int join)
{
	zval *ret = NULL, **header;
	char *key = php_http_pretty_key(estrndup(key_str, key_len), key_len, 1, 1);

	if (SUCCESS == zend_symtable_find(&msg->hdrs, key, key_len + 1, (void *) &header)) {
		if (join && Z_TYPE_PP(header) == IS_ARRAY) {
			TSRMLS_FETCH_FROM_CTX(msg->ts);

			ret = php_http_header_value_to_string(*header TSRMLS_CC);
		} else {
			Z_ADDREF_PP(header);
			ret = *header;
		}
	}

	efree(key);

	return ret;
}

PHP_HTTP_API zend_bool php_http_message_is_multipart(php_http_message_t *msg, char **boundary)
{
	zval *ct = php_http_message_header(msg, ZEND_STRL("Content-Type"), 1);
	zend_bool is_multipart = 0;
	TSRMLS_FETCH_FROM_CTX(msg->ts);

	if (ct) {
		php_http_params_opts_t popts;
		HashTable params;

		ZEND_INIT_SYMTABLE(&params);
		php_http_params_opts_default_get(&popts);
		popts.input.str = Z_STRVAL_P(ct);
		popts.input.len = Z_STRLEN_P(ct);

		if (php_http_params_parse(&params, &popts TSRMLS_CC)) {
			zval **cur, **arg;
			char *ct_str;

			zend_hash_internal_pointer_reset(&params);

			if (SUCCESS == zend_hash_get_current_data(&params, (void *) &cur)
			&&	Z_TYPE_PP(cur) == IS_ARRAY
			&&	HASH_KEY_IS_STRING == zend_hash_get_current_key(&params, &ct_str, NULL, 0)
			) {
				if (php_http_match(ct_str, "multipart", PHP_HTTP_MATCH_WORD)) {
					is_multipart = 1;

					/* get boundary */
					if (boundary
					&&	SUCCESS == zend_hash_find(Z_ARRVAL_PP(cur), ZEND_STRS("arguments"), (void *) &arg)
					&&	Z_TYPE_PP(arg) == IS_ARRAY
					) {
						zval **val;
						HashPosition pos;
						php_http_array_hashkey_t key = php_http_array_hashkey_init(0);

						FOREACH_KEYVAL(pos, *arg, key, val) {
							if (key.type == HASH_KEY_IS_STRING && !strcasecmp(key.str, "boundary")) {
								zval *bnd = php_http_ztyp(IS_STRING, *val);

								if (Z_STRLEN_P(bnd)) {
									*boundary = estrndup(Z_STRVAL_P(bnd), Z_STRLEN_P(bnd));
								}
								zval_ptr_dtor(&bnd);
							}
						}
					}
				}
			}
		}
		zend_hash_destroy(&params);
		zval_ptr_dtor(&ct);
	}

	return is_multipart;
}

/* */
PHP_HTTP_API void php_http_message_set_type(php_http_message_t *message, php_http_message_type_t type)
{
	/* just act if different */
	if (type != message->type) {

		/* free request info */
		switch (message->type) {
			case PHP_HTTP_REQUEST:
				STR_FREE(message->http.info.request.method);
				STR_FREE(message->http.info.request.url);
				break;
			
			case PHP_HTTP_RESPONSE:
				STR_FREE(message->http.info.response.status);
				break;
			
			default:
				break;
		}

		message->type = type;
		memset(&message->http, 0, sizeof(message->http));
	}
}

PHP_HTTP_API void php_http_message_set_info(php_http_message_t *message, php_http_info_t *info)
{
	php_http_message_set_type(message, info->type);
	message->http.version = info->http.version;
	switch (message->type) {
		case PHP_HTTP_REQUEST:
			STR_SET(PHP_HTTP_INFO(message).request.url, PHP_HTTP_INFO(info).request.url ? estrdup(PHP_HTTP_INFO(info).request.url) : NULL);
			STR_SET(PHP_HTTP_INFO(message).request.method, PHP_HTTP_INFO(info).request.method ? estrdup(PHP_HTTP_INFO(info).request.method) : NULL);
			break;
		
		case PHP_HTTP_RESPONSE:
			PHP_HTTP_INFO(message).response.code = PHP_HTTP_INFO(info).response.code;
			STR_SET(PHP_HTTP_INFO(message).response.status, PHP_HTTP_INFO(info).response.status ? estrdup(PHP_HTTP_INFO(info).response.status) : NULL);
			break;
		
		default:
			break;
	}
}

PHP_HTTP_API void php_http_message_update_headers(php_http_message_t *msg)
{
	zval *h;
	size_t size;

	if (php_http_message_body_stream(msg->body)->readfilters.head) {
		/* if a read stream filter is attached to the body the caller must also care for the headers */
	} else if ((size = php_http_message_body_size(msg->body))) {
		MAKE_STD_ZVAL(h);
		ZVAL_LONG(h, size);
		zend_hash_update(&msg->hdrs, "Content-Length", sizeof("Content-Length"), &h, sizeof(zval *), NULL);

		if (msg->body->boundary) {
			char *str;
			size_t len;

			if (!(h = php_http_message_header(msg, ZEND_STRL("Content-Type"), 1))) {
				len = spprintf(&str, 0, "multipart/form-data; boundary=\"%s\"", msg->body->boundary);
				MAKE_STD_ZVAL(h);
				ZVAL_STRINGL(h, str, len, 0);
				zend_hash_update(&msg->hdrs, "Content-Type", sizeof("Content-Type"), &h, sizeof(zval *), NULL);
			} else if (!php_http_match(Z_STRVAL_P(h), "boundary=", PHP_HTTP_MATCH_WORD)) {
				zval_dtor(h);
				Z_STRLEN_P(h) = spprintf(&Z_STRVAL_P(h), 0, "%s; boundary=\"%s\"", Z_STRVAL_P(h), msg->body->boundary);
				zend_hash_update(&msg->hdrs, "Content-Type", sizeof("Content-Type"), &h, sizeof(zval *), NULL);
			} else {
				zval_ptr_dtor(&h);
			}
		}
	}
}

static void message_headers(php_http_message_t *msg, php_http_buffer_t *str)
{
	TSRMLS_FETCH_FROM_CTX(msg->ts);

	switch (msg->type) {
		case PHP_HTTP_REQUEST:
			php_http_buffer_appendf(str, PHP_HTTP_INFO_REQUEST_FMT_ARGS(&msg->http, PHP_HTTP_CRLF));
			break;

		case PHP_HTTP_RESPONSE:
			php_http_buffer_appendf(str, PHP_HTTP_INFO_RESPONSE_FMT_ARGS(&msg->http, PHP_HTTP_CRLF));
			break;

		default:
			break;
	}

	php_http_message_update_headers(msg);
	php_http_headers_to_string(str, &msg->hdrs TSRMLS_CC);
}

PHP_HTTP_API void php_http_message_to_callback(php_http_message_t *msg, php_http_pass_callback_t cb, void *cb_arg)
{
	php_http_buffer_t str;

	php_http_buffer_init_ex(&str, 0x1000, 0);
	message_headers(msg, &str);
	cb(cb_arg, str.data, str.used);
	php_http_buffer_dtor(&str);

	if (php_http_message_body_size(msg->body)) {
		cb(cb_arg, ZEND_STRL(PHP_HTTP_CRLF));
		php_http_message_body_to_callback(msg->body, cb, cb_arg, 0, 0);
	}
}

PHP_HTTP_API void php_http_message_to_string(php_http_message_t *msg, char **string, size_t *length)
{
	php_http_buffer_t str;
	char *data;

	php_http_buffer_init_ex(&str, 0x1000, 0);
	message_headers(msg, &str);
	if (php_http_message_body_size(msg->body)) {
		php_http_buffer_appends(&str, PHP_HTTP_CRLF);
		php_http_message_body_to_callback(msg->body, (php_http_pass_callback_t) php_http_buffer_append, &str, 0, 0);
	}

	data = php_http_buffer_data(&str, string, length);
	if (!string) {
		efree(data);
	}

	php_http_buffer_dtor(&str);
}

PHP_HTTP_API void php_http_message_serialize(php_http_message_t *message, char **string, size_t *length)
{
	char *buf;
	php_http_buffer_t str;
	php_http_message_t *msg;

	php_http_buffer_init(&str);

	msg = message = php_http_message_reverse(message);
	do {
		php_http_message_to_callback(message, (php_http_pass_callback_t) php_http_buffer_append, &str);
		php_http_buffer_appends(&str, PHP_HTTP_CRLF);
	} while ((message = message->parent));
	php_http_message_reverse(msg);

	buf = php_http_buffer_data(&str, string, length);
	if (!string) {
		efree(buf);
	}

	php_http_buffer_dtor(&str);
}

PHP_HTTP_API php_http_message_t *php_http_message_reverse(php_http_message_t *msg)
{
	int i, c = 0;
	
	php_http_message_count(c, msg);
	
	if (c > 1) {
		php_http_message_t *tmp = msg, **arr;

		arr = ecalloc(c, sizeof(**arr));
		for (i = 0; i < c; ++i) {
			arr[i] = tmp;
			tmp = tmp->parent;
		}
		arr[0]->parent = NULL;
		for (i = 0; i < c-1; ++i) {
			arr[i+1]->parent = arr[i];
		}
		
		msg = arr[c-1];
		efree(arr);
	}
	
	return msg;
}

PHP_HTTP_API php_http_message_t *php_http_message_zip(php_http_message_t *one, php_http_message_t *two)
{
	php_http_message_t *dst = php_http_message_copy(one, NULL), *src = php_http_message_copy(two, NULL), *tmp_dst, *tmp_src, *ret = dst;

	while(dst && src) {
		tmp_dst = dst->parent;
		tmp_src = src->parent;
		dst->parent = src;
		if (tmp_dst) {
			src->parent = tmp_dst;
		}
		src = tmp_src;
		dst = tmp_dst;
	}

	return ret;
}

PHP_HTTP_API php_http_message_t *php_http_message_copy_ex(php_http_message_t *from, php_http_message_t *to, zend_bool parents)
{
	php_http_message_t *temp, *copy = NULL;
	php_http_info_t info;
	TSRMLS_FETCH_FROM_CTX(from->ts);
	
	if (from) {
		info.type = from->type;
		info.http = from->http;
		
		copy = temp = php_http_message_init(to, 0, php_http_message_body_copy(from->body, NULL) TSRMLS_CC);
		php_http_message_set_info(temp, &info);
		zend_hash_copy(&temp->hdrs, &from->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	
		if (parents) while (from->parent) {
			info.type = from->parent->type;
			info.http = from->parent->http;
		
			temp->parent = php_http_message_init(NULL, 0, php_http_message_body_copy(from->parent->body, NULL) TSRMLS_CC);
			php_http_message_set_info(temp->parent, &info);
			zend_hash_copy(&temp->parent->hdrs, &from->parent->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		
			temp = temp->parent;
			from = from->parent;
		}
	}
	
	return copy;
}

PHP_HTTP_API php_http_message_t *php_http_message_copy(php_http_message_t *from, php_http_message_t *to)
{
	return php_http_message_copy_ex(from, to, 1);
}

PHP_HTTP_API void php_http_message_dtor(php_http_message_t *message)
{
	if (message) {
		zend_hash_destroy(&message->hdrs);
		php_http_message_body_free(&message->body);
		
		switch (message->type) {
			case PHP_HTTP_REQUEST:
				STR_SET(message->http.info.request.method, NULL);
				STR_SET(message->http.info.request.url, NULL);
				break;
			
			case PHP_HTTP_RESPONSE:
				STR_SET(message->http.info.response.status, NULL);
				break;
			
			default:
				break;
		}
	}
}

PHP_HTTP_API void php_http_message_free(php_http_message_t **message)
{
	if (*message) {
		if ((*message)->parent) {
			php_http_message_free(&(*message)->parent);
		}
		php_http_message_dtor(*message);
		efree(*message);
		*message = NULL;
	}
}

static zval *php_http_message_object_read_prop(zval *object, zval *member, int type PHP_HTTP_ZEND_LITERAL_DC TSRMLS_DC);
static void php_http_message_object_write_prop(zval *object, zval *member, zval *value PHP_HTTP_ZEND_LITERAL_DC TSRMLS_DC);
static HashTable *php_http_message_object_get_props(zval *object TSRMLS_DC);

static zend_object_handlers php_http_message_object_handlers;
static HashTable php_http_message_object_prophandlers;

typedef void (*php_http_message_object_prophandler_func_t)(php_http_message_object_t *o, zval *v TSRMLS_DC);

typedef struct php_http_message_object_prophandler {
	php_http_message_object_prophandler_func_t read;
	php_http_message_object_prophandler_func_t write;
} php_http_message_object_prophandler_t;

static STATUS php_http_message_object_add_prophandler(const char *prop_str, size_t prop_len, php_http_message_object_prophandler_func_t read, php_http_message_object_prophandler_func_t write) {
	php_http_message_object_prophandler_t h = { read, write };
	return zend_hash_add(&php_http_message_object_prophandlers, prop_str, prop_len + 1, (void *) &h, sizeof(h), NULL);
}
static STATUS php_http_message_object_get_prophandler(const char *prop_str, size_t prop_len, php_http_message_object_prophandler_t **handler) {
	return zend_hash_find(&php_http_message_object_prophandlers, prop_str, prop_len + 1, (void *) handler);
}
static void php_http_message_object_prophandler_get_type(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	RETVAL_LONG(obj->message->type);
}
static void php_http_message_object_prophandler_set_type(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	zval *cpy = php_http_ztyp(IS_LONG, value);
	php_http_message_set_type(obj->message, Z_LVAL_P(cpy));
	zval_ptr_dtor(&cpy);
}
static void php_http_message_object_prophandler_get_request_method(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, obj->message) && obj->message->http.info.request.method) {
		RETVAL_STRING(obj->message->http.info.request.method, 1);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_request_method(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, obj->message)) {
		zval *cpy = php_http_ztyp(IS_STRING, value);
		STR_SET(obj->message->http.info.request.method, estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy)));
		zval_ptr_dtor(&cpy);
	}
}
static void php_http_message_object_prophandler_get_request_url(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, obj->message) && obj->message->http.info.request.url) {
		RETVAL_STRING(obj->message->http.info.request.url, 1);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_request_url(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, obj->message)) {
		zval *cpy = php_http_ztyp(IS_STRING, value);
		STR_SET(obj->message->http.info.request.url, estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy)));
		zval_ptr_dtor(&cpy);
	}
}
static void php_http_message_object_prophandler_get_response_status(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, obj->message) && obj->message->http.info.response.status) {
		RETVAL_STRING(obj->message->http.info.response.status, 1);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_response_status(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, obj->message)) {
		zval *cpy = php_http_ztyp(IS_STRING, value);
		STR_SET(obj->message->http.info.response.status, estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy)));
		zval_ptr_dtor(&cpy);
	}
}
static void php_http_message_object_prophandler_get_response_code(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, obj->message)) {
		RETVAL_LONG(obj->message->http.info.response.code);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_response_code(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, obj->message)) {
		zval *cpy = php_http_ztyp(IS_LONG, value);
		obj->message->http.info.response.code = Z_LVAL_P(cpy);
		STR_SET(obj->message->http.info.response.status, estrdup(php_http_env_get_response_status_for_code(obj->message->http.info.response.code)));
		zval_ptr_dtor(&cpy);
	}
}
static void php_http_message_object_prophandler_get_http_version(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	char *version_str;
	size_t version_len;

	php_http_version_to_string(&obj->message->http.version, &version_str, &version_len, NULL, NULL TSRMLS_CC);
	RETVAL_STRINGL(version_str, version_len, 0);
}
static void php_http_message_object_prophandler_set_http_version(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	zval *cpy = php_http_ztyp(IS_STRING, value);
	php_http_version_parse(&obj->message->http.version, Z_STRVAL_P(cpy) TSRMLS_CC);
	zval_ptr_dtor(&cpy);
}
static void php_http_message_object_prophandler_get_headers(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	array_init(return_value);
	zend_hash_copy(Z_ARRVAL_P(return_value), &obj->message->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
}
static void php_http_message_object_prophandler_set_headers(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	zval *cpy = php_http_ztyp(IS_ARRAY, value);

	zend_hash_clean(&obj->message->hdrs);
	zend_hash_copy(&obj->message->hdrs, Z_ARRVAL_P(cpy), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	zval_ptr_dtor(&cpy);
}
static void php_http_message_object_prophandler_get_body(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	if (obj->body) {
		RETVAL_OBJVAL(obj->body->zv, 1);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_body(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	php_http_message_object_set_body(obj, value TSRMLS_CC);
}
static void php_http_message_object_prophandler_get_parent_message(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	if (obj->message->parent) {
		RETVAL_OBJVAL(obj->parent->zv, 1);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_parent_message(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	if (Z_TYPE_P(value) == IS_OBJECT && instanceof_function(Z_OBJCE_P(value), php_http_message_class_entry TSRMLS_CC)) {
		php_http_message_object_t *parent_obj = zend_object_store_get_object(value TSRMLS_CC);

		if (obj->message->parent) {
			zend_objects_store_del_ref_by_handle(obj->parent->zv.handle TSRMLS_CC);
		}
		Z_OBJ_ADDREF_P(value);
		obj->parent = parent_obj;
		obj->message->parent = parent_obj->message;
	}
}

#define PHP_HTTP_MESSAGE_OBJECT_INIT(obj) \
	do { \
		if (!obj->message) { \
			obj->message = php_http_message_init(NULL, 0, NULL TSRMLS_CC); \
		} \
	} while(0)


void php_http_message_object_reverse(zval *this_ptr, zval *return_value TSRMLS_DC)
{
	int i = 0;
	php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	/* count */
	php_http_message_count(i, obj->message);

	if (i > 1) {
		php_http_message_object_t **objects;
		int last;

		objects = ecalloc(i, sizeof(**objects));

		/* we are the first message */
		objects[0] = obj;

		/* fetch parents */
		for (i = 1; obj->parent; ++i) {
			 objects[i] = obj = obj->parent;
		}

		/* reorder parents */
		for (last = --i; i; --i) {
			objects[i]->message->parent = objects[i-1]->message;
			objects[i]->parent = objects[i-1];
		}

		objects[0]->message->parent = NULL;
		objects[0]->parent = NULL;

		/* add ref, because we previously have not been a parent message */
		Z_OBJ_ADDREF_P(getThis());
		RETVAL_OBJVAL(objects[last]->zv, 0);

		efree(objects);
	} else {
		RETURN_ZVAL(getThis(), 1, 0);
	}
}

void php_http_message_object_prepend(zval *this_ptr, zval *prepend, zend_bool top TSRMLS_DC)
{
	zval m;
	php_http_message_t *save_parent_msg = NULL;
	php_http_message_object_t *save_parent_obj = NULL, *obj = zend_object_store_get_object(this_ptr TSRMLS_CC);
	php_http_message_object_t *prepend_obj = zend_object_store_get_object(prepend TSRMLS_CC);

	INIT_PZVAL(&m);
	m.type = IS_OBJECT;

	if (!top) {
		save_parent_obj = obj->parent;
		save_parent_msg = obj->message->parent;
	} else {
		/* iterate to the most parent object */
		while (obj->parent) {
			obj = obj->parent;
		}
	}

	/* prepend */
	obj->parent = prepend_obj;
	obj->message->parent = prepend_obj->message;

	/* add ref */
	zend_objects_store_add_ref(prepend TSRMLS_CC);
	/*
	while (prepend_obj->parent) {
		m.value.obj = prepend_obj->parent->zv;
		zend_objects_store_add_ref(&m TSRMLS_CC);
		prepend_obj = zend_object_store_get_object(&m TSRMLS_CC);
	}
	*/
	if (!top) {
		prepend_obj->parent = save_parent_obj;
		prepend_obj->message->parent = save_parent_msg;
	}
}

STATUS php_http_message_object_set_body(php_http_message_object_t *msg_obj, zval *zbody TSRMLS_DC)
{
	zval *tmp = NULL;
	php_stream *s;
	zend_object_value ov;
	php_http_message_body_t *body;
	php_http_message_body_object_t *body_obj;

	switch (Z_TYPE_P(zbody)) {
		case IS_RESOURCE:
			php_stream_from_zval_no_verify(s, &zbody);
			if (!s) {
				php_http_error(HE_THROW, PHP_HTTP_E_CLIENT, "not a valid stream resource");
				return FAILURE;
			}

			is_resource:

			body = php_http_message_body_init(NULL, s TSRMLS_CC);
			if (SUCCESS != php_http_new(&ov, php_http_message_body_class_entry, (php_http_new_t) php_http_message_body_object_new_ex, NULL, body, NULL TSRMLS_CC)) {
				php_http_message_body_free(&body);
				return FAILURE;
			}
			MAKE_STD_ZVAL(tmp);
			ZVAL_OBJVAL(tmp, ov, 0);
			zbody = tmp;
			break;

		case IS_OBJECT:
			if (instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_class_entry TSRMLS_CC)) {
				Z_OBJ_ADDREF_P(zbody);
				break;
			}
			/* no break */

		default:
			tmp = php_http_ztyp(IS_STRING, zbody);
			s = php_stream_temp_new();
			php_stream_write(s, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
			zval_ptr_dtor(&tmp);
			tmp = NULL;
			goto is_resource;

	}

	body_obj = zend_object_store_get_object(zbody TSRMLS_CC);

	if (msg_obj->body) {
		zend_objects_store_del_ref_by_handle(msg_obj->body->zv.handle TSRMLS_CC);
	}
	php_http_message_body_free(&msg_obj->message->body);

	msg_obj->message->body = php_http_message_body_init(&body_obj->body, NULL TSRMLS_CC);
	msg_obj->body = body_obj;

	if (tmp) {
		FREE_ZVAL(tmp);
	}
	return SUCCESS;
}

zend_object_value php_http_message_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_message_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_message_object_new_ex(zend_class_entry *ce, php_http_message_t *msg, php_http_message_object_t **ptr TSRMLS_DC)
{
	php_http_message_object_t *o;

	o = ecalloc(1, sizeof(php_http_message_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (ptr) {
		*ptr = o;
	}

	if (msg) {
		o->message = msg;
		if (msg->parent) {
			php_http_message_object_new_ex(ce, msg->parent, &o->parent TSRMLS_CC);
		}
		php_http_message_body_object_new_ex(php_http_message_body_class_entry, php_http_message_body_init(&msg->body, NULL TSRMLS_CC), &o->body TSRMLS_CC);
	}

	o->zv.handle = zend_objects_store_put((zend_object *) o, NULL, php_http_message_object_free, NULL TSRMLS_CC);
	o->zv.handlers = &php_http_message_object_handlers;

	return o->zv;
}

zend_object_value php_http_message_object_clone(zval *this_ptr TSRMLS_DC)
{
	zend_object_value new_ov;
	php_http_message_object_t *new_obj = NULL;
	php_http_message_object_t *old_obj = zend_object_store_get_object(this_ptr TSRMLS_CC);

	new_ov = php_http_message_object_new_ex(old_obj->zo.ce, php_http_message_copy(old_obj->message, NULL), &new_obj TSRMLS_CC);
	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);

	return new_ov;
}

void php_http_message_object_free(void *object TSRMLS_DC)
{
	php_http_message_object_t *o = (php_http_message_object_t *) object;

	if (o->iterator) {
		zval_ptr_dtor(&o->iterator);
		o->iterator = NULL;
	}
	if (o->message) {
		/* do NOT free recursivly */
		php_http_message_dtor(o->message);
		efree(o->message);
		o->message = NULL;
	}
	if (o->parent) {
		zend_objects_store_del_ref_by_handle(o->parent->zv.handle TSRMLS_CC);
		o->parent = NULL;
	}
	if (o->body) {
		zend_objects_store_del_ref_by_handle(o->body->zv.handle TSRMLS_CC);
		o->body = NULL;
	}
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

static zval *php_http_message_object_read_prop(zval *object, zval *member, int type PHP_HTTP_ZEND_LITERAL_DC TSRMLS_DC)
{
	php_http_message_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);
	php_http_message_object_prophandler_t *handler;
	zval *return_value, *copy = php_http_ztyp(IS_STRING, member);

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (SUCCESS == php_http_message_object_get_prophandler(Z_STRVAL_P(copy), Z_STRLEN_P(copy), &handler)) {
		ALLOC_ZVAL(return_value);
		Z_SET_REFCOUNT_P(return_value, 0);
		Z_UNSET_ISREF_P(return_value);

		if (type == BP_VAR_R) {
			handler->read(obj, return_value TSRMLS_CC);
		} else {
			php_property_proxy_t *proxy = php_property_proxy_init(object, Z_STRVAL_P(copy), Z_STRLEN_P(copy) TSRMLS_CC);
			RETVAL_OBJVAL(php_property_proxy_object_new_ex(php_property_proxy_get_class_entry(), proxy, NULL TSRMLS_CC), 0);
		}
	} else {
		return_value = zend_get_std_object_handlers()->read_property(object, member, type PHP_HTTP_ZEND_LITERAL_CC TSRMLS_CC);
	}

	zval_ptr_dtor(&copy);

	return return_value;
}

static void php_http_message_object_write_prop(zval *object, zval *member, zval *value PHP_HTTP_ZEND_LITERAL_DC TSRMLS_DC)
{
	php_http_message_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);
	php_http_message_object_prophandler_t *handler;
	zval *copy = php_http_ztyp(IS_STRING, member);

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (SUCCESS == php_http_message_object_get_prophandler(Z_STRVAL_P(copy), Z_STRLEN_P(copy), &handler)) {
		handler->write(obj, value TSRMLS_CC);
	} else {
		zend_get_std_object_handlers()->write_property(object, member, value PHP_HTTP_ZEND_LITERAL_CC TSRMLS_CC);
	}

	zval_ptr_dtor(&copy);
}

static HashTable *php_http_message_object_get_props(zval *object TSRMLS_DC)
{
	zval *headers;
	php_http_message_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);
	php_http_message_t *msg = obj->message;
	HashTable *props = zend_get_std_object_handlers()->get_properties(object TSRMLS_CC);
	zval array, *parent, *body;
	char *version;

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);
	
	INIT_PZVAL_ARRAY(&array, props);
	
#define ASSOC_PROP(ptype, n, val) \
	do { \
		zend_property_info *pi; \
		if (SUCCESS == zend_hash_find(&obj->zo.ce->properties_info, n, sizeof(n), (void *) &pi)) { \
			add_assoc_ ##ptype## _ex(&array, pi->name, pi->name_length + 1, val); \
		} \
	} while(0) \

#define ASSOC_STRING(name, val) ASSOC_STRINGL(name, val, strlen(val))
#define ASSOC_STRINGL(name, val, len) ASSOC_STRINGL_EX(name, val, len, 1)
#define ASSOC_STRINGL_EX(n, val, len, cpy) \
	do { \
		zend_property_info *pi; \
		if (SUCCESS == zend_hash_find(&obj->zo.ce->properties_info, n, sizeof(n), (void *) &pi)) { \
			add_assoc_stringl_ex(&array, pi->name, pi->name_length + 1, val, len, cpy); \
		} \
	} while(0)

	ASSOC_PROP(long, "type", msg->type);
	ASSOC_STRINGL_EX("httpVersion", version, spprintf(&version, 0, "%u.%u", msg->http.version.major, msg->http.version.minor), 0);

	switch (msg->type) {
		case PHP_HTTP_REQUEST:
			ASSOC_PROP(long, "responseCode", 0);
			ASSOC_STRINGL("responseStatus", "", 0);
			ASSOC_STRING("requestMethod", STR_PTR(msg->http.info.request.method));
			ASSOC_STRING("requestUrl", STR_PTR(msg->http.info.request.url));
			break;

		case PHP_HTTP_RESPONSE:
			ASSOC_PROP(long, "responseCode", msg->http.info.response.code);
			ASSOC_STRING("responseStatus", STR_PTR(msg->http.info.response.status));
			ASSOC_STRINGL("requestMethod", "", 0);
			ASSOC_STRINGL("requestUrl", "", 0);
			break;

		case PHP_HTTP_NONE:
		default:
			ASSOC_PROP(long, "responseCode", 0);
			ASSOC_STRINGL("responseStatus", "", 0);
			ASSOC_STRINGL("requestMethod", "", 0);
			ASSOC_STRINGL("requestUrl", "", 0);
			break;
	}

	MAKE_STD_ZVAL(headers);
	array_init(headers);
	zend_hash_copy(Z_ARRVAL_P(headers), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	ASSOC_PROP(zval, "headers", headers);

	MAKE_STD_ZVAL(body);
	if (!obj->body) {
		php_http_new(NULL, php_http_message_body_class_entry, (php_http_new_t) php_http_message_body_object_new_ex, NULL, (void *) php_http_message_body_init(&obj->message->body, NULL TSRMLS_CC), (void *) &obj->body TSRMLS_CC);
	}
	ZVAL_OBJVAL(body, obj->body->zv, 1);
	ASSOC_PROP(zval, "body", body);

	MAKE_STD_ZVAL(parent);
	if (msg->parent) {
		ZVAL_OBJVAL(parent, obj->parent->zv, 1);
	} else {
		ZVAL_NULL(parent);
	}
	ASSOC_PROP(zval, "parentMessage", parent);

	return props;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, message)
	ZEND_ARG_INFO(0, greedy)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, __construct)
{
	zend_bool greedy = 1;
	zval *zmessage = NULL;
	php_http_message_t *msg = NULL;
	php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z!b", &zmessage, &greedy) && zmessage) {
			if (Z_TYPE_P(zmessage) == IS_RESOURCE) {
				php_stream *s;
				php_http_message_parser_t p;

				php_stream_from_zval(s, &zmessage);
				if (s && php_http_message_parser_init(&p TSRMLS_CC)) {
					unsigned flags = (greedy ? PHP_HTTP_MESSAGE_PARSER_GREEDY : 0);
					
					php_http_message_parser_parse_stream(&p, s, flags, &msg);
					php_http_message_parser_dtor(&p);
				}
				
				if (!msg) {
					php_http_error(HE_THROW, PHP_HTTP_E_MESSAGE, "could not parse message from stream");
				}
			} else {
				zmessage = php_http_ztyp(IS_STRING, zmessage);
				if (!(msg = php_http_message_parse(NULL, Z_STRVAL_P(zmessage), Z_STRLEN_P(zmessage), greedy TSRMLS_CC))) {
					php_http_error(HE_THROW, PHP_HTTP_E_MESSAGE, "could not parse message: %.*s", MIN(25, Z_STRLEN_P(zmessage)), Z_STRVAL_P(zmessage));
				}
				zval_ptr_dtor(&zmessage);
			}

			if (msg) {
				php_http_message_dtor(obj->message);
				obj->message = msg;
				if (obj->message->parent) {
					php_http_message_object_new_ex(Z_OBJCE_P(getThis()), obj->message->parent, &obj->parent TSRMLS_CC);
				}
			}
		}
		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);
	} end_error_handling();

}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getBody, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getBody)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

			if (!obj->body) {
				php_http_message_body_addref(obj->message->body);
				php_http_new(NULL, php_http_message_body_class_entry, (php_http_new_t) php_http_message_body_object_new_ex, NULL, obj->message->body, (void *) &obj->body TSRMLS_CC);
			}
			if (obj->body) {
				RETVAL_OBJVAL(obj->body->zv, 1);
			}
		}
	} end_error_handling();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setBody, 0, 0, 1)
	ZEND_ARG_INFO(0, body)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setBody)
{
	zval *zbody;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &zbody, php_http_message_body_class_entry)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);
		php_http_message_object_prophandler_set_body(obj, zbody TSRMLS_CC);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_addBody, 0, 0, 1)
	ZEND_ARG_INFO(0, body)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, addBody)
{
	zval *new_body;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &new_body, php_http_message_body_class_entry)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_http_message_body_object_t *new_obj = zend_object_store_get_object(new_body TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);
		php_http_message_body_to_callback(new_obj->body, (php_http_pass_callback_t) php_http_message_body_append, obj->message->body, 0, 0);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getHeader, 0, 0, 1)
	ZEND_ARG_INFO(0, header)
	ZEND_ARG_INFO(0, into_class)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getHeader)
{
	char *header_str;
	int header_len;
	zend_class_entry *header_ce = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|C!", &header_str, &header_len, &header_ce)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		zval *header;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if ((header = php_http_message_header(obj->message, header_str, header_len, 0))) {
			if (!header_ce) {
				RETURN_ZVAL(header, 1, 1);
			} else if (instanceof_function(header_ce, php_http_header_class_entry TSRMLS_CC)) {
				zval *header_name, **argv[2];

				MAKE_STD_ZVAL(header_name);
				ZVAL_STRINGL(header_name, header_str, header_len, 1);
				Z_ADDREF_P(header);

				argv[0] = &header_name;
				argv[1] = &header;

				object_init_ex(return_value, header_ce);
				php_http_method_call(return_value, ZEND_STRL("__construct"), 2, argv, NULL TSRMLS_CC);

				zval_ptr_dtor(&header_name);
				zval_ptr_dtor(&header);

				return;
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "Class '%s' is not as descendant of http\\Header", header_ce->name);
			}
		}
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getHeaders, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getHeaders)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		array_init(return_value);
		array_copy(&obj->message->hdrs, Z_ARRVAL_P(return_value));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setHeader, 0, 0, 1)
	ZEND_ARG_INFO(0, header)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setHeader)
{
	zval *zvalue = NULL;
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z!", &name_str, &name_len, &zvalue)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *name = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (!zvalue) {
			zend_symtable_del(&obj->message->hdrs, name, name_len + 1);
		} else {
			Z_ADDREF_P(zvalue);
			zend_symtable_update(&obj->message->hdrs, name, name_len + 1, &zvalue, sizeof(void *), NULL);
		}
		efree(name);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setHeaders, 0, 0, 1)
	ZEND_ARG_INFO(0, headers)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setHeaders)
{
	zval *new_headers = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/!", &new_headers)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		zend_hash_clean(&obj->message->hdrs);
		if (new_headers) {
			array_join(Z_ARRVAL_P(new_headers), &obj->message->hdrs, 0, ARRAY_JOIN_PRETTIFY|ARRAY_JOIN_STRONLY);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_addHeader, 0, 0, 2)
	ZEND_ARG_INFO(0, header)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, addHeader)
{
	zval *zvalue;
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name_str, &name_len, &zvalue)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *name = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);
		zval *header;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		Z_ADDREF_P(zvalue);
		if ((header = php_http_message_header(obj->message, name, name_len, 0))) {
			convert_to_array(header);
			zend_hash_next_index_insert(Z_ARRVAL_P(header), &zvalue, sizeof(void *), NULL);
			zval_ptr_dtor(&header);
		} else {
			zend_symtable_update(&obj->message->hdrs, name, name_len + 1, &zvalue, sizeof(void *), NULL);
		}
		efree(name);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_addHeaders, 0, 0, 1)
	ZEND_ARG_INFO(0, headers)
	ZEND_ARG_INFO(0, append)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, addHeaders)
{
	zval *new_headers;
	zend_bool append = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|b", &new_headers, &append)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		array_join(Z_ARRVAL_P(new_headers), &obj->message->hdrs, append, ARRAY_JOIN_STRONLY|ARRAY_JOIN_PRETTIFY);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getType, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getType)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		RETURN_LONG(obj->message->type);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setType, 0, 0, 1)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setType)
{
	long type;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_http_message_set_type(obj->message, type);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getInfo, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getInfo)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		switch (obj->message->type) {
			case PHP_HTTP_REQUEST:
				Z_STRLEN_P(return_value) = spprintf(&Z_STRVAL_P(return_value), 0, PHP_HTTP_INFO_REQUEST_FMT_ARGS(&obj->message->http, ""));
				break;
			case PHP_HTTP_RESPONSE:
				Z_STRLEN_P(return_value) = spprintf(&Z_STRVAL_P(return_value), 0, PHP_HTTP_INFO_RESPONSE_FMT_ARGS(&obj->message->http, ""));
				break;
			default:
				RETURN_NULL();
				break;
		}
		Z_TYPE_P(return_value) = IS_STRING;
		return;
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setInfo, 0, 0, 1)
	ZEND_ARG_INFO(0, http_info)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setInfo)
{
	char *str;
	int len;
	php_http_info_t inf;

	if (	SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &len)
	&&		php_http_info_parse(&inf, str TSRMLS_CC)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_http_message_set_info(obj->message, &inf);
		php_http_info_dtor(&inf);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getHttpVersion, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getHttpVersion)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		char *str;
		size_t len;
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_http_version_to_string(&obj->message->http.version, &str, &len, NULL, NULL TSRMLS_CC);
		RETURN_STRINGL(str, len, 0);
	}

	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setHttpVersion, 0, 0, 1)
	ZEND_ARG_INFO(0, http_version)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setHttpVersion)
{
	char *v_str;
	int v_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &v_str, &v_len)) {
		php_http_version_t version;
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (php_http_version_parse(&version, v_str TSRMLS_CC)) {
			obj->message->http.version = version;
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getResponseCode, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getResponseCode)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		PHP_HTTP_MESSAGE_TYPE_CHECK(RESPONSE, obj->message, RETURN_FALSE);
		RETURN_LONG(obj->message->http.info.response.code);
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setResponseCode, 0, 0, 1)
	ZEND_ARG_INFO(0, response_code)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setResponseCode)
{
	long code;
	zend_bool strict = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|b", &code, &strict)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		PHP_HTTP_MESSAGE_TYPE_CHECK(RESPONSE, obj->message, RETURN_FALSE);
		if (strict && (code < 100 || code > 599)) {
			php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "Invalid response code (100-599): %ld", code);
			RETURN_FALSE;
		}

		obj->message->http.info.response.code = code;
		STR_SET(obj->message->http.info.response.status, estrdup(php_http_env_get_response_status_for_code(code)));
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getResponseStatus, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getResponseStatus)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		PHP_HTTP_MESSAGE_TYPE_CHECK(RESPONSE, obj->message, RETURN_FALSE);
		if (obj->message->http.info.response.status) {
			RETURN_STRING(obj->message->http.info.response.status, 1);
		} else {
			RETURN_EMPTY_STRING();
		}
	}

	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setResponseStatus, 0, 0, 1)
	ZEND_ARG_INFO(0, response_status)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setResponseStatus)
{
	char *status;
	int status_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &status, &status_len)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		PHP_HTTP_MESSAGE_TYPE_CHECK(RESPONSE, obj->message, RETURN_FALSE);
		STR_SET(obj->message->http.info.response.status, estrndup(status, status_len));
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getRequestMethod, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getRequestMethod)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		PHP_HTTP_MESSAGE_TYPE_CHECK(REQUEST, obj->message, RETURN_FALSE);
		if (obj->message->http.info.request.method) {
			RETURN_STRING(obj->message->http.info.request.method, 1);
		} else {
			RETURN_EMPTY_STRING();
		}
	}

	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setRequestMethod, 0, 0, 1)
	ZEND_ARG_INFO(0, request_method)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setRequestMethod)
{
	char *method;
	int method_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &method, &method_len)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		PHP_HTTP_MESSAGE_TYPE_CHECK(REQUEST, obj->message, RETURN_FALSE);
		if (method_len < 1) {
			php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "Cannot set HttpMessage::requestMethod to an empty string");
			RETURN_FALSE;
		}

		STR_SET(obj->message->http.info.request.method, estrndup(method, method_len));
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getRequestUrl, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getRequestUrl)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		PHP_HTTP_MESSAGE_TYPE_CHECK(REQUEST, obj->message, RETURN_FALSE);
		if (obj->message->http.info.request.url) {
			RETURN_STRING(obj->message->http.info.request.url, 1);
		} else {
			RETURN_EMPTY_STRING();
		}
	}

	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setRequestUrl, 0, 0, 1)
	ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setRequestUrl)
{
	char *url_str;
	int url_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &url_str, &url_len)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		PHP_HTTP_MESSAGE_TYPE_CHECK(REQUEST, obj->message, RETURN_FALSE);
		if (url_len < 1) {
			php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "Cannot set HttpMessage::requestUrl to an empty string");
			RETURN_FALSE;
		}
		STR_SET(obj->message->http.info.request.url, estrndup(url_str, url_len));
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getParentMessage, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getParentMessage)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

			if (obj->message->parent) {
				RETVAL_OBJVAL(obj->parent->zv, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "HttpMessage does not have a parent message");
			}
		}
	} end_error_handling();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage___toString, 0, 0, 0)
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_toString, 0, 0, 0)
	ZEND_ARG_INFO(0, include_parent)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, toString)
{
	zend_bool include_parent = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &include_parent)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *string;
		size_t length;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (include_parent) {
			php_http_message_serialize(obj->message, &string, &length);
		} else {
			php_http_message_to_string(obj->message, &string, &length);
		}
		if (string) {
			RETURN_STRINGL(string, length, 0);
		}
	}
	RETURN_EMPTY_STRING();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_toStream, 0, 0, 1)
	ZEND_ARG_INFO(0, stream)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, toStream)
{
	zval *zstream;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zstream)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_stream *s;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_stream_from_zval(s, &zstream);
		php_http_message_to_callback(obj->message, (php_http_pass_callback_t) _php_stream_write, s);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_toCallback, 0, 0, 1)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, toCallback)
{
	php_http_pass_fcall_arg_t fcd;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "f", &fcd.fci, &fcd.fcc)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		fcd.fcz = getThis();
		Z_ADDREF_P(fcd.fcz);
		TSRMLS_SET_CTX(fcd.ts);

		php_http_message_to_callback(obj->message, php_http_pass_fcall_callback, &fcd);
		zend_fcall_info_args_clear(&fcd.fci, 1);

		zval_ptr_dtor(&fcd.fcz);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_serialize, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, serialize)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *string;
		size_t length;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_http_message_serialize(obj->message, &string, &length);
		RETURN_STRINGL(string, length, 0);
	}
	RETURN_EMPTY_STRING();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_unserialize, 0, 0, 1)
	ZEND_ARG_INFO(0, serialized)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, unserialize)
{
	int length;
	char *serialized;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &serialized, &length)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_http_message_t *msg;

		if (obj->message) {
			php_http_message_dtor(obj->message);
			efree(obj->message);
		}
		if ((msg = php_http_message_parse(NULL, serialized, (size_t) length, 1 TSRMLS_CC))) {
			obj->message = msg;
		} else {
			obj->message = php_http_message_init(NULL, 0, NULL TSRMLS_CC);
			php_http_error(HE_ERROR, PHP_HTTP_E_RUNTIME, "Could not unserialize HttpMessage");
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_detach, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, detach)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

			RETVAL_OBJVAL(php_http_message_object_new_ex(obj->zo.ce, php_http_message_copy_ex(obj->message, NULL, 0), NULL TSRMLS_CC), 0);
		}
	} end_error_handling();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_prepend, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, message, http\\Message, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, prepend)
{
	zval *prepend;
	zend_bool top = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|b", &prepend, php_http_message_class_entry, &top)) {
		php_http_message_t *msg[2];
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_http_message_object_t *prepend_obj = zend_object_store_get_object(prepend TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (!prepend_obj->message) {
			prepend_obj->message = php_http_message_init(NULL, 0, NULL TSRMLS_CC);
		}

		/* safety check */
		for (msg[0] = obj->message; msg[0]; msg[0] = msg[0]->parent) {
			for (msg[1] = prepend_obj->message; msg[1]; msg[1] = msg[1]->parent) {
				if (msg[0] == msg[1]) {
					php_http_error(HE_THROW, PHP_HTTP_E_INVALID_PARAM, "Cannot prepend a message located within the same message chain");
					return;
				}
			}
		}

		php_http_message_object_prepend(getThis(), prepend, top TSRMLS_CC);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_reverse, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, reverse)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_reverse(getThis(), return_value TSRMLS_CC);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_isMultipart, 0, 0, 0)
	ZEND_ARG_INFO(1, boundary)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, isMultipart)
{
	zval *zboundary = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &zboundary)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *boundary = NULL;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		RETVAL_BOOL(php_http_message_is_multipart(obj->message, zboundary ? &boundary : NULL));

		if (zboundary && boundary) {
			zval_dtor(zboundary);
			ZVAL_STRING(zboundary, boundary, 0);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_splitMultipartBody, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, splitMultipartBody)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *boundary = NULL;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (php_http_message_is_multipart(obj->message, &boundary)) {
			php_http_message_t *msg;

			if ((msg = php_http_message_body_split(obj->message->body, boundary))) {
				RETVAL_OBJVAL(php_http_message_object_new_ex(php_http_message_class_entry, msg, NULL TSRMLS_CC), 0);
			}
		}
		STR_FREE(boundary);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_count, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, count)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		long i = 0;
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_http_message_count(i, obj->message);
		RETURN_LONG(i);
	}
	RETURN_FALSE;
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_rewind, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, rewind)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *zobj = getThis();
		php_http_message_object_t *obj = zend_object_store_get_object(zobj TSRMLS_CC);

		if (obj->iterator) {
			zval_ptr_dtor(&obj->iterator);
		}
		Z_ADDREF_P(zobj);
		obj->iterator = zobj;
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_valid, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, valid)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_BOOL(obj->iterator != NULL);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_next, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, next)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->iterator) {
			php_http_message_object_t *itr = zend_object_store_get_object(obj->iterator TSRMLS_CC);

			if (itr && itr->parent) {
				zval *old = obj->iterator;
				MAKE_STD_ZVAL(obj->iterator);
				ZVAL_OBJVAL(obj->iterator, itr->parent->zv, 1);
				zval_ptr_dtor(&old);
			} else {
				zval_ptr_dtor(&obj->iterator);
				obj->iterator = NULL;
			}
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_key, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, key)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG(obj->iterator ? obj->iterator->value.obj.handle:0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_current, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, current)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->iterator) {
			RETURN_ZVAL(obj->iterator, 1, 0);
		}
	}
}

static zend_function_entry php_http_message_methods[] = {
	PHP_ME(HttpMessage, __construct,        ai_HttpMessage___construct,        ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpMessage, getBody,            ai_HttpMessage_getBody,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setBody,            ai_HttpMessage_setBody,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, addBody,            ai_HttpMessage_addBody,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getHeader,          ai_HttpMessage_getHeader,          ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setHeader,          ai_HttpMessage_setHeader,          ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, addHeader,          ai_HttpMessage_addHeader,          ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getHeaders,         ai_HttpMessage_getHeaders,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setHeaders,         ai_HttpMessage_setHeaders,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, addHeaders,         ai_HttpMessage_addHeaders,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getType,            ai_HttpMessage_getType,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setType,            ai_HttpMessage_setType,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getInfo,            ai_HttpMessage_getInfo,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setInfo,            ai_HttpMessage_setInfo,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getResponseCode,    ai_HttpMessage_getResponseCode,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setResponseCode,    ai_HttpMessage_setResponseCode,    ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getResponseStatus,  ai_HttpMessage_getResponseStatus,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setResponseStatus,  ai_HttpMessage_setResponseStatus,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getRequestMethod,   ai_HttpMessage_getRequestMethod,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setRequestMethod,   ai_HttpMessage_setRequestMethod,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getRequestUrl,      ai_HttpMessage_getRequestUrl,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setRequestUrl,      ai_HttpMessage_setRequestUrl,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getHttpVersion,     ai_HttpMessage_getHttpVersion,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, setHttpVersion,     ai_HttpMessage_setHttpVersion,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, getParentMessage,   ai_HttpMessage_getParentMessage,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, toString,           ai_HttpMessage_toString,           ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, toCallback,         ai_HttpMessage_toCallback,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, toStream,           ai_HttpMessage_toStream,           ZEND_ACC_PUBLIC)

	/* implements Countable */
	PHP_ME(HttpMessage, count,              ai_HttpMessage_count,              ZEND_ACC_PUBLIC)

	/* implements Serializable */
	PHP_ME(HttpMessage, serialize,          ai_HttpMessage_serialize,          ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, unserialize,        ai_HttpMessage_unserialize,        ZEND_ACC_PUBLIC)

	/* implements Iterator */
	PHP_ME(HttpMessage, rewind,             ai_HttpMessage_rewind,             ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, valid,              ai_HttpMessage_valid,              ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, current,            ai_HttpMessage_current,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, key,                ai_HttpMessage_key,                ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, next,               ai_HttpMessage_next,               ZEND_ACC_PUBLIC)

	ZEND_MALIAS(HttpMessage, __toString, toString, ai_HttpMessage___toString,  ZEND_ACC_PUBLIC)

	PHP_ME(HttpMessage, detach,             ai_HttpMessage_detach,             ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, prepend,            ai_HttpMessage_prepend,            ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, reverse,            ai_HttpMessage_reverse,            ZEND_ACC_PUBLIC)

	PHP_ME(HttpMessage, isMultipart,        ai_HttpMessage_isMultipart,        ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessage, splitMultipartBody, ai_HttpMessage_splitMultipartBody, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};

zend_class_entry *php_http_message_class_entry;

PHP_MINIT_FUNCTION(http_message)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Message", php_http_message_methods);
	php_http_message_class_entry = zend_register_internal_class_ex(&ce, php_http_object_class_entry, NULL TSRMLS_CC);
	php_http_message_class_entry->create_object = php_http_message_object_new;
	memcpy(&php_http_message_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_message_object_handlers.clone_obj = php_http_message_object_clone;
	php_http_message_object_handlers.read_property = php_http_message_object_read_prop;
	php_http_message_object_handlers.write_property = php_http_message_object_write_prop;
	php_http_message_object_handlers.get_properties = php_http_message_object_get_props;
	php_http_message_object_handlers.get_property_ptr_ptr = NULL;

	zend_class_implements(php_http_message_class_entry TSRMLS_CC, 3, spl_ce_Countable, zend_ce_serializable, zend_ce_iterator);

	zend_hash_init(&php_http_message_object_prophandlers, 9, NULL, NULL, 1);
	zend_declare_property_long(php_http_message_class_entry, ZEND_STRL("type"), PHP_HTTP_NONE, ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("type"), php_http_message_object_prophandler_get_type, php_http_message_object_prophandler_set_type);
	zend_declare_property_null(php_http_message_class_entry, ZEND_STRL("body"), ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("body"), php_http_message_object_prophandler_get_body, php_http_message_object_prophandler_set_body);
	zend_declare_property_string(php_http_message_class_entry, ZEND_STRL("requestMethod"), "", ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("requestMethod"), php_http_message_object_prophandler_get_request_method, php_http_message_object_prophandler_set_request_method);
	zend_declare_property_string(php_http_message_class_entry, ZEND_STRL("requestUrl"), "", ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("requestUrl"), php_http_message_object_prophandler_get_request_url, php_http_message_object_prophandler_set_request_url);
	zend_declare_property_string(php_http_message_class_entry, ZEND_STRL("responseStatus"), "", ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("responseStatus"), php_http_message_object_prophandler_get_response_status, php_http_message_object_prophandler_set_response_status);
	zend_declare_property_long(php_http_message_class_entry, ZEND_STRL("responseCode"), 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("responseCode"), php_http_message_object_prophandler_get_response_code, php_http_message_object_prophandler_set_response_code);
	zend_declare_property_null(php_http_message_class_entry, ZEND_STRL("httpVersion"), ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("httpVersion"), php_http_message_object_prophandler_get_http_version, php_http_message_object_prophandler_set_http_version);
	zend_declare_property_null(php_http_message_class_entry, ZEND_STRL("headers"), ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("headers"), php_http_message_object_prophandler_get_headers, php_http_message_object_prophandler_set_headers);
	zend_declare_property_null(php_http_message_class_entry, ZEND_STRL("parentMessage"), ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("parentMessage"), php_http_message_object_prophandler_get_parent_message, php_http_message_object_prophandler_set_parent_message);

	zend_declare_class_constant_long(php_http_message_class_entry, ZEND_STRL("TYPE_NONE"), PHP_HTTP_NONE TSRMLS_CC);
	zend_declare_class_constant_long(php_http_message_class_entry, ZEND_STRL("TYPE_REQUEST"), PHP_HTTP_REQUEST TSRMLS_CC);
	zend_declare_class_constant_long(php_http_message_class_entry, ZEND_STRL("TYPE_RESPONSE"), PHP_HTTP_RESPONSE TSRMLS_CC);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_message)
{
	zend_hash_destroy(&php_http_message_object_prophandlers);

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

