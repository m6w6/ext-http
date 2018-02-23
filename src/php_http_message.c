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

static void message_headers(php_http_message_t *msg, php_http_buffer_t *str);

zend_bool php_http_message_info_callback(php_http_message_t **message, HashTable **headers, php_http_info_t *info)
{
	php_http_message_t *old = *message;

	/* advance message */
	if (!old || old->type || zend_hash_num_elements(&old->hdrs)) {
		(*message) = php_http_message_init(NULL, 0, NULL);
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

php_http_message_t *php_http_message_init(php_http_message_t *message, php_http_message_type_t type, php_http_message_body_t *body)
{
	if (!message) {
		message = emalloc(sizeof(*message));
	}
	memset(message, 0, sizeof(*message));

	php_http_message_set_type(message, type);
	message->http.version.major = 1;
	message->http.version.minor = 1;
	zend_hash_init(&message->hdrs, 0, NULL, ZVAL_PTR_DTOR, 0);
	message->body = body ? body : php_http_message_body_init(NULL, NULL);

	return message;
}

php_http_message_t *php_http_message_init_env(php_http_message_t *message, php_http_message_type_t type)
{
	int free_msg = !message;
	zval *sval, tval;
	php_http_message_body_t *mbody;

	switch (type) {
		case PHP_HTTP_REQUEST:
			mbody = php_http_env_get_request_body();
			php_http_message_body_addref(mbody);
			message = php_http_message_init(message, type, mbody);
			if ((sval = php_http_env_get_server_var(ZEND_STRL("SERVER_PROTOCOL"), 1)) && !strncmp(Z_STRVAL_P(sval), "HTTP/", lenof("HTTP/"))) {
				php_http_version_parse(&message->http.version, Z_STRVAL_P(sval));
			}
			if ((sval = php_http_env_get_server_var(ZEND_STRL("REQUEST_METHOD"), 1))) {
				message->http.info.request.method = estrdup(Z_STRVAL_P(sval));
			}
			if ((sval = php_http_env_get_server_var(ZEND_STRL("REQUEST_URI"), 1))) {
				message->http.info.request.url = php_http_url_parse(Z_STRVAL_P(sval), Z_STRLEN_P(sval), PHP_HTTP_URL_STDFLAGS);
			}

			php_http_env_get_request_headers(&message->hdrs);
			break;

		case PHP_HTTP_RESPONSE:
			message = php_http_message_init(message, type, NULL);
			if (!SG(sapi_headers).http_status_line || !php_http_info_parse((php_http_info_t *) &message->http, SG(sapi_headers).http_status_line)) {
				if (!(message->http.info.response.code = SG(sapi_headers).http_response_code)) {
					message->http.info.response.code = 200;
				}
				message->http.info.response.status = estrdup(php_http_env_get_response_status_for_code(message->http.info.response.code));
			}

			php_http_env_get_response_headers(&message->hdrs);
			if (php_output_get_level()) {
				if (php_output_get_status() & PHP_OUTPUT_SENT) {
					php_error_docref(NULL, E_WARNING, "Could not fetch response body, output has already been sent at %s:%d", php_output_get_start_filename(), php_output_get_start_lineno());

					goto error;
				} else if (SUCCESS != php_output_get_contents(&tval)) {
					php_error_docref(NULL, E_WARNING, "Could not fetch response body");
					goto error;
				} else {
					php_http_message_body_append(message->body, Z_STRVAL(tval), Z_STRLEN(tval));
					zval_dtor(&tval);
				}
			}
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

php_http_message_t *php_http_message_parse(php_http_message_t *msg, const char *str, size_t len, zend_bool greedy)
{
	php_http_message_parser_t p;
	php_http_buffer_t buf;
	unsigned flags = PHP_HTTP_MESSAGE_PARSER_CLEANUP;
	int free_msg;

	php_http_buffer_from_string_ex(&buf, str, len);
	php_http_message_parser_init(&p);

	if ((free_msg = !msg)) {
		msg = php_http_message_init(NULL, 0, NULL);
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

zval *php_http_message_header(php_http_message_t *msg, const char *key_str, size_t key_len)
{
	zval *ret;
	char *key;
	ALLOCA_FLAG(free_key);

	key = do_alloca(key_len + 1, free_key);

	memcpy(key, key_str, key_len);
	key[key_len] = '\0';
	php_http_pretty_key(key, key_len, 1, 1);

	ret = zend_symtable_str_find(&msg->hdrs, key, key_len);

	free_alloca(key, free_key);

	return ret;
}

zend_bool php_http_message_is_multipart(php_http_message_t *msg, char **boundary)
{
	zend_string *ct = php_http_message_header_string(msg, ZEND_STRL("Content-Type"));
	zend_bool is_multipart = 0;

	if (ct) {
		php_http_params_opts_t popts;
		HashTable params;

		ZEND_INIT_SYMTABLE(&params);
		php_http_params_opts_default_get(&popts);
		popts.input.str = ct->val;
		popts.input.len = ct->len;

		if (EXPECTED(php_http_params_parse(&params, &popts))) {
			zval *cur, *arg;
			zend_string *ct_str;
			zend_ulong index;

			zend_hash_internal_pointer_reset(&params);

			if (EXPECTED((cur = zend_hash_get_current_data(&params))
			&&	(Z_TYPE_P(cur) == IS_ARRAY)
			&&	(HASH_KEY_IS_STRING == zend_hash_get_current_key(&params, &ct_str, &index)))
			) {
				if (php_http_match(ct_str->val, "multipart", PHP_HTTP_MATCH_WORD)) {
					is_multipart = 1;

					/* get boundary */
					if (EXPECTED(boundary
					&&	(arg = zend_hash_str_find(Z_ARRVAL_P(cur), ZEND_STRL("arguments")))
					&&	Z_TYPE_P(arg) == IS_ARRAY)
					) {
						zval *val;
						php_http_arrkey_t key;

						ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(arg), key.h, key.key, val)
						{
							if (key.key && key.key->len == lenof("boundary") && !strcasecmp(key.key->val, "boundary")) {
								zend_string *bnd = zval_get_string(val);

								if (EXPECTED(bnd->len)) {
									*boundary = estrndup(bnd->val, bnd->len);
								}
								zend_string_release(bnd);
							}
						}
						ZEND_HASH_FOREACH_END();
					}
				}
			}
		}
		zend_hash_destroy(&params);
		zend_string_release(ct);
	}

	return is_multipart;
}

/* */
void php_http_message_set_type(php_http_message_t *message, php_http_message_type_t type)
{
	/* just act if different */
	if (type != message->type) {

		/* free request info */
		switch (message->type) {
			case PHP_HTTP_REQUEST:
				PTR_FREE(message->http.info.request.method);
				PTR_FREE(message->http.info.request.url);
				break;

			case PHP_HTTP_RESPONSE:
				PTR_FREE(message->http.info.response.status);
				break;

			default:
				break;
		}

		message->type = type;
		memset(&message->http, 0, sizeof(message->http));
	}
}

void php_http_message_set_info(php_http_message_t *message, php_http_info_t *info)
{
	php_http_message_set_type(message, info->type);
	message->http.version = info->http.version;
	switch (message->type) {
		case PHP_HTTP_REQUEST:
			PTR_SET(PHP_HTTP_INFO(message).request.url, PHP_HTTP_INFO(info).request.url ? php_http_url_copy(PHP_HTTP_INFO(info).request.url, 0) : NULL);
			PTR_SET(PHP_HTTP_INFO(message).request.method, PHP_HTTP_INFO(info).request.method ? estrdup(PHP_HTTP_INFO(info).request.method) : NULL);
			break;

		case PHP_HTTP_RESPONSE:
			PHP_HTTP_INFO(message).response.code = PHP_HTTP_INFO(info).response.code;
			PTR_SET(PHP_HTTP_INFO(message).response.status, PHP_HTTP_INFO(info).response.status ? estrdup(PHP_HTTP_INFO(info).response.status) : NULL);
			break;

		default:
			break;
	}
}

void php_http_message_update_headers(php_http_message_t *msg)
{
	zval h;
	size_t size;
	zend_string *cl;

	if (php_http_message_body_stream(msg->body)->readfilters.head) {
		/* if a read stream filter is attached to the body the caller must also care for the headers */
	} else if (php_http_message_header(msg, ZEND_STRL("Content-Range"))) {
		/* don't mess around with a Content-Range message */
	} else if ((size = php_http_message_body_size(msg->body))) {
		ZVAL_LONG(&h, size);
		zend_hash_str_update(&msg->hdrs, "Content-Length", lenof("Content-Length"), &h);

		if (msg->body->boundary) {
			char *str;
			size_t len;
			zend_string *ct;

			if (!(ct = php_http_message_header_string(msg, ZEND_STRL("Content-Type")))) {
				len = spprintf(&str, 0, "multipart/form-data; boundary=\"%s\"", msg->body->boundary);
				ZVAL_STR(&h, php_http_cs2zs(str, len));
				zend_hash_str_update(&msg->hdrs, "Content-Type", lenof("Content-Type"), &h);
			} else if (!php_http_match(ct->val, "boundary=", PHP_HTTP_MATCH_WORD)) {
				len = spprintf(&str, 0, "%s; boundary=\"%s\"", ct->val, msg->body->boundary);
				ZVAL_STR(&h, php_http_cs2zs(str, len));
				zend_hash_str_update(&msg->hdrs, "Content-Type", lenof("Content-Type"), &h);
				zend_string_release(ct);
			} else {
				zend_string_release(ct);
			}
		}
	} else if ((cl = php_http_message_header_string(msg, ZEND_STRL("Content-Length")))) {
		if (!zend_string_equals_literal(cl, "0")) {
			/* body->size == 0, so get rid of old Content-Length */
			zend_hash_str_del(&msg->hdrs, ZEND_STRL("Content-Length"));
		}
		zend_string_release(cl);
	} else if (msg->type == PHP_HTTP_REQUEST) {
		if (!php_http_message_header(msg, ZEND_STRL("Transfer-Encoding"))) {
			/* no filter, no CR, no size, no TE, no CL */
			if (0 <= php_http_select_str(msg->http.info.request.method, 3, "POST", "PUT", "PATCH")) {
				/* quoting RFC7230#section-3.3.2
					A user agent SHOULD send a Content-Length in a request message when
					no Transfer-Encoding is sent and the request method defines a meaning
					for an enclosed payload body.  For example, a Content-Length header
					field is normally sent in a POST request even when the value is 0
					(indicating an empty payload body).  A user agent SHOULD NOT send a
					Content-Length header field when the request message does not contain
					a payload body and the method semantics do not anticipate such a
					body.
				*/
				ZVAL_LONG(&h, 0);
				zend_hash_str_update(&msg->hdrs, "Content-Length", lenof("Content-Length"), &h);
			}
		}
	}
}

static void message_headers(php_http_message_t *msg, php_http_buffer_t *str)
{
	char *tmp = NULL;
	size_t len = 0;

	php_http_info_to_string((php_http_info_t *) msg, &tmp, &len, PHP_HTTP_CRLF TSRMLS_CC);
	php_http_message_update_headers(msg);

	php_http_buffer_append(str, tmp, len);
	php_http_header_to_string(str, &msg->hdrs);
	PTR_FREE(tmp);
}

void php_http_message_to_callback(php_http_message_t *msg, php_http_pass_callback_t cb, void *cb_arg)
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

void php_http_message_to_string(php_http_message_t *msg, char **string, size_t *length)
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

void php_http_message_serialize(php_http_message_t *message, char **string, size_t *length)
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

php_http_message_t *php_http_message_reverse(php_http_message_t *msg)
{
	size_t i, c = php_http_message_count(msg);

	if (c > 1) {
		php_http_message_t *tmp = msg, **arr;

		arr = ecalloc(c, sizeof(*arr));
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

php_http_message_t *php_http_message_zip(php_http_message_t *dst, php_http_message_t *src)
{
	php_http_message_t *tmp_dst, *tmp_src, *ret = dst;

	while (dst && src) {
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

php_http_message_t *php_http_message_copy_ex(php_http_message_t *from, php_http_message_t *to, zend_bool parents)
{
	php_http_message_t *temp, *copy = NULL;
	php_http_info_t info;

	if (from) {
		info.type = from->type;
		info.http = from->http;

		copy = temp = php_http_message_init(to, 0, php_http_message_body_copy(from->body, NULL));
		php_http_message_set_info(temp, &info);
		array_copy(&from->hdrs, &temp->hdrs);

		if (parents) while (from->parent) {
			info.type = from->parent->type;
			info.http = from->parent->http;

			temp->parent = php_http_message_init(NULL, 0, php_http_message_body_copy(from->parent->body, NULL));
			php_http_message_set_info(temp->parent, &info);
			array_copy(&from->parent->hdrs, &temp->parent->hdrs);

			temp = temp->parent;
			from = from->parent;
		}
	}

	return copy;
}

void php_http_message_dtor(php_http_message_t *message)
{
	if (EXPECTED(message)) {
		zend_hash_destroy(&message->hdrs);
		php_http_message_body_free(&message->body);

		switch (message->type) {
			case PHP_HTTP_REQUEST:
				PTR_SET(message->http.info.request.method, NULL);
				PTR_SET(message->http.info.request.url, NULL);
				break;

			case PHP_HTTP_RESPONSE:
				PTR_SET(message->http.info.response.status, NULL);
				break;

			default:
				break;
		}
	}
}

void php_http_message_free(php_http_message_t **message)
{
	if (EXPECTED(*message)) {
		if ((*message)->parent) {
			php_http_message_free(&(*message)->parent);
		}
		php_http_message_dtor(*message);
		efree(*message);
		*message = NULL;
	}
}

static zend_class_entry *php_http_message_class_entry;
zend_class_entry *php_http_message_get_class_entry(void)
{
	return php_http_message_class_entry;
}

static zval *php_http_message_object_read_prop(zval *object, zval *member, int type, void **cache_slot, zval *rv);
static void php_http_message_object_write_prop(zval *object, zval *member, zval *value, void **cache_slot);

static zend_object_handlers php_http_message_object_handlers;
static HashTable php_http_message_object_prophandlers;

static void php_http_message_object_prophandler_hash_dtor(zval *pData)
{
	pefree(Z_PTR_P(pData), 1);
}

typedef void (*php_http_message_object_prophandler_func_t)(php_http_message_object_t *o, zval *v);

typedef struct php_http_message_object_prophandler {
	php_http_message_object_prophandler_func_t read;
	php_http_message_object_prophandler_func_t write;
} php_http_message_object_prophandler_t;

static ZEND_RESULT_CODE php_http_message_object_add_prophandler(const char *prop_str, size_t prop_len, php_http_message_object_prophandler_func_t read, php_http_message_object_prophandler_func_t write) {
	php_http_message_object_prophandler_t h = { read, write };
	if (!zend_hash_str_add_mem(&php_http_message_object_prophandlers, prop_str, prop_len, (void *) &h, sizeof(h))) {
		return FAILURE;
	}
	return SUCCESS;
}
static php_http_message_object_prophandler_t *php_http_message_object_get_prophandler(zend_string *name_str) {
	return zend_hash_str_find_ptr(&php_http_message_object_prophandlers, name_str->val, name_str->len);
}
static void php_http_message_object_prophandler_get_type(php_http_message_object_t *obj, zval *return_value) {
	zval_ptr_dtor(return_value);
	RETVAL_LONG(obj->message->type);
}
static void php_http_message_object_prophandler_set_type(php_http_message_object_t *obj, zval *value) {
	php_http_message_set_type(obj->message, zval_get_long(value));
}
static void php_http_message_object_prophandler_get_request_method(php_http_message_object_t *obj, zval *return_value) {
	zval_ptr_dtor(return_value);
	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, obj->message) && obj->message->http.info.request.method) {
		RETVAL_STRING(obj->message->http.info.request.method);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_request_method(php_http_message_object_t *obj, zval *value) {
	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, obj->message)) {
		zend_string *zs = zval_get_string(value);
		PTR_SET(obj->message->http.info.request.method, estrndup(zs->val, zs->len));
		zend_string_release(zs);
	}
}
static void php_http_message_object_prophandler_get_request_url(php_http_message_object_t *obj, zval *return_value) {
	char *url_str;
	size_t url_len;

	zval_ptr_dtor(return_value);
	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, obj->message) && obj->message->http.info.request.url && php_http_url_to_string(obj->message->http.info.request.url, &url_str, &url_len, 0)) {
		RETVAL_STR(php_http_cs2zs(url_str, url_len));
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_request_url(php_http_message_object_t *obj, zval *value) {
	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, obj->message)) {
		PTR_SET(obj->message->http.info.request.url, php_http_url_from_zval(value, PHP_HTTP_URL_STDFLAGS));
	}
}
static void php_http_message_object_prophandler_get_response_status(php_http_message_object_t *obj, zval *return_value) {
	zval_ptr_dtor(return_value);
	if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, obj->message) && obj->message->http.info.response.status) {
		RETVAL_STRING(obj->message->http.info.response.status);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_response_status(php_http_message_object_t *obj, zval *value) {
	if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, obj->message)) {
		zend_string *zs = zval_get_string(value);
		PTR_SET(obj->message->http.info.response.status, estrndup(zs->val, zs->len));
		zend_string_release(zs);
	}
}
static void php_http_message_object_prophandler_get_response_code(php_http_message_object_t *obj, zval *return_value) {
	zval_ptr_dtor(return_value);
	if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, obj->message)) {
		RETVAL_LONG(obj->message->http.info.response.code);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_response_code(php_http_message_object_t *obj, zval *value) {
	if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, obj->message)) {
		obj->message->http.info.response.code = zval_get_long(value);
		PTR_SET(obj->message->http.info.response.status, estrdup(php_http_env_get_response_status_for_code(obj->message->http.info.response.code)));
	}
}
static void php_http_message_object_prophandler_get_http_version(php_http_message_object_t *obj, zval *return_value) {
	char *version_str;
	size_t version_len;

	zval_ptr_dtor(return_value);
	php_http_version_to_string(&obj->message->http.version, &version_str, &version_len, NULL, NULL);
	RETVAL_STR(php_http_cs2zs(version_str, version_len));
}
static void php_http_message_object_prophandler_set_http_version(php_http_message_object_t *obj, zval *value) {
	zend_string *zs = zval_get_string(value);
	php_http_version_parse(&obj->message->http.version, zs->val);
	zend_string_release(zs);
}
static void php_http_message_object_prophandler_get_headers(php_http_message_object_t *obj, zval *return_value ) {
	zval tmp;

	ZVAL_COPY_VALUE(&tmp, return_value);
	array_init(return_value);
	array_copy(&obj->message->hdrs, Z_ARRVAL_P(return_value));
	zval_ptr_dtor(&tmp);
}
static void php_http_message_object_prophandler_set_headers(php_http_message_object_t *obj, zval *value) {
	int converted = 0;
	HashTable garbage, *src;

	if (Z_TYPE_P(value) != IS_ARRAY && Z_TYPE_P(value) != IS_OBJECT) {
		converted = 1;
		SEPARATE_ZVAL(value);
		convert_to_array(value);
	}
	src = HASH_OF(value);

	garbage = obj->message->hdrs;
	zend_hash_init(&obj->message->hdrs, zend_hash_num_elements(src), NULL, ZVAL_PTR_DTOR, 0);
	array_copy(HASH_OF(value), &obj->message->hdrs);

	zend_hash_destroy(&garbage);

	if (converted) {
		zval_ptr_dtor(value);
	}
}
static void php_http_message_object_prophandler_get_body(php_http_message_object_t *obj, zval *return_value) {
	if (obj->body) {
		zval tmp;

		ZVAL_COPY_VALUE(&tmp, return_value);
		RETVAL_OBJECT(&obj->body->zo, 1);
		zval_ptr_dtor(&tmp);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_body(php_http_message_object_t *obj, zval *value) {
	php_http_message_object_set_body(obj, value);
}
static void php_http_message_object_prophandler_get_parent_message(php_http_message_object_t *obj, zval *return_value) {
	if (obj->message->parent) {
		zval tmp;

		ZVAL_COPY_VALUE(&tmp, return_value);
		RETVAL_OBJECT(&obj->parent->zo, 1);
		zval_ptr_dtor(&tmp);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_parent_message(php_http_message_object_t *obj, zval *value) {
	if (Z_TYPE_P(value) == IS_OBJECT && instanceof_function(Z_OBJCE_P(value), php_http_message_class_entry)) {
		php_http_message_object_t *parent_obj = PHP_HTTP_OBJ(NULL, value);

		Z_ADDREF_P(value);
		if (obj->message->parent) {
			zend_object_release(&obj->parent->zo);
		}
		obj->parent = parent_obj;
		obj->message->parent = parent_obj->message;
	}
}

#define PHP_HTTP_MESSAGE_OBJECT_INIT(obj) \
	do { \
		if (!obj->message) { \
			obj->message = php_http_message_init(NULL, 0, NULL); \
		} \
	} while(0)


void php_http_message_object_reverse(zval *zmsg, zval *return_value)
{
	size_t i;
	php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, zmsg);

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	/* count */
	i = php_http_message_count(obj->message);

	if (i > 1) {
		php_http_message_object_t **objects;
		int last;

		objects = ecalloc(i, sizeof(*objects));

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
		Z_ADDREF_P(zmsg);
		/* no addref, because we've been a parent message previously */
		RETVAL_OBJECT(&objects[last]->zo, 0);

		efree(objects);
	} else {
		RETURN_ZVAL(zmsg, 1, 0);
	}
}

void php_http_message_object_prepend(zval *this_ptr, zval *prepend, zend_bool top)
{
	php_http_message_t *save_parent_msg = NULL;
	php_http_message_object_t *save_parent_obj = NULL, *obj = PHP_HTTP_OBJ(NULL, this_ptr);
	php_http_message_object_t *prepend_obj = PHP_HTTP_OBJ(NULL, prepend);

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
	Z_ADDREF_P(prepend);

	if (!top) {
		prepend_obj->parent = save_parent_obj;
		prepend_obj->message->parent = save_parent_msg;
	}
}

ZEND_RESULT_CODE php_http_message_object_set_body(php_http_message_object_t *msg_obj, zval *zbody)
{
	php_stream *s;
	zend_string *body_str;
	php_http_message_body_t *body;
	php_http_message_body_object_t *body_obj;

	switch (Z_TYPE_P(zbody)) {
		case IS_RESOURCE:
			php_stream_from_zval_no_verify(s, zbody);
			if (!s) {
				php_http_throw(unexpected_val, "The stream is not a valid resource", NULL);
				return FAILURE;
			}

			is_resource:

			body = php_http_message_body_init(NULL, s);
			if (!(body_obj = php_http_message_body_object_new_ex(php_http_get_message_body_class_entry(), body))) {
				php_http_message_body_free(&body);
				return FAILURE;
			}
			break;

		case IS_OBJECT:
			if (instanceof_function(Z_OBJCE_P(zbody), php_http_get_message_body_class_entry())) {
				Z_ADDREF_P(zbody);
				body_obj = PHP_HTTP_OBJ(NULL, zbody);
				break;
			}
			/* no break */

		default:
			body_str = zval_get_string(zbody);
			s = php_stream_temp_new();
			php_stream_write(s, body_str->val, body_str->len);
			zend_string_release(body_str);
			goto is_resource;

	}

	if (!body_obj->body) {
		body_obj->body = php_http_message_body_init(NULL, NULL);
	}
	if (msg_obj->body) {
		zend_object_release(&msg_obj->body->zo);
	}
	if (msg_obj->message) {
		php_http_message_body_free(&msg_obj->message->body);
		msg_obj->message->body = body_obj->body;
	} else {
		msg_obj->message = php_http_message_init(NULL, 0, body_obj->body);
	}
	php_http_message_body_addref(body_obj->body);
	msg_obj->body = body_obj;

	return SUCCESS;
}

ZEND_RESULT_CODE php_http_message_object_init_body_object(php_http_message_object_t *obj)
{
	php_http_message_body_addref(obj->message->body);
	return php_http_new((void *) &obj->body, php_http_get_message_body_class_entry(), (php_http_new_t) php_http_message_body_object_new_ex, NULL, obj->message->body);
}

zend_object *php_http_message_object_new(zend_class_entry *ce)
{
	return &php_http_message_object_new_ex(ce, NULL)->zo;
}

php_http_message_object_t *php_http_message_object_new_ex(zend_class_entry *ce, php_http_message_t *msg)
{
	php_http_message_object_t *o;

	o = ecalloc(1, sizeof(*o) + zend_object_properties_size(ce));
	zend_object_std_init(&o->zo, ce);
	object_properties_init(&o->zo, ce);

	if (msg) {
		o->message = msg;
		if (msg->parent) {
			o->parent = php_http_message_object_new_ex(ce, msg->parent);
		}
		o->body = php_http_message_body_object_new_ex(php_http_get_message_body_class_entry(), php_http_message_body_init(&msg->body, NULL));
	}

	o->zo.handlers = &php_http_message_object_handlers;

	return o;
}

zend_object *php_http_message_object_clone(zval *this_ptr)
{
	php_http_message_object_t *new_obj;
	php_http_message_object_t *old_obj = PHP_HTTP_OBJ(NULL, this_ptr);

	new_obj = php_http_message_object_new_ex(old_obj->zo.ce, php_http_message_copy(old_obj->message, NULL));
	zend_objects_clone_members(&new_obj->zo, &old_obj->zo);

	return &new_obj->zo;
}

void php_http_message_object_free(zend_object *object)
{
	php_http_message_object_t *o = PHP_HTTP_OBJ(object, NULL);

	PTR_FREE(o->gc);

	if (!Z_ISUNDEF(o->iterator)) {
		zval_ptr_dtor(&o->iterator);
		ZVAL_UNDEF(&o->iterator);
	}
	if (o->message) {
		/* do NOT free recursivly */
		php_http_message_dtor(o->message);
		efree(o->message);
		o->message = NULL;
	}
	if (o->parent) {
		zend_object_release(&o->parent->zo);
		o->parent = NULL;
	}
	if (o->body) {
		zend_object_release(&o->body->zo);
		o->body = NULL;
	}
	zend_object_std_dtor(object);
}

static zval *php_http_message_object_read_prop(zval *object, zval *member, int type, void **cache_slot, zval *tmp)
{
	zval *return_value;
	zend_string *member_name = zval_get_string(member);
	php_http_message_object_prophandler_t *handler = php_http_message_object_get_prophandler(member_name);

	return_value = zend_get_std_object_handlers()->read_property(object, member, type, cache_slot, tmp);

	if (handler && handler->read) {
		if (type == BP_VAR_R || type == BP_VAR_IS) {
			php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, object);

			handler->read(obj, return_value);
		} else {
			php_property_proxy_t *proxy;
			php_property_proxy_object_t *proxy_obj;

			proxy = php_property_proxy_init(object, member_name);
			proxy_obj = php_property_proxy_object_new_ex(NULL, proxy);

			ZVAL_OBJ(tmp, &proxy_obj->zo);
			return_value = tmp;
		}
	}

	zend_string_release(member_name);
	return return_value;
}

static void php_http_message_object_write_prop(zval *object, zval *member, zval *value, void **cache_slot)
{
	php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, object);
	php_http_message_object_prophandler_t *handler;
	zend_string *member_name = zval_get_string(member);

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if ((handler = php_http_message_object_get_prophandler(member_name))) {
		handler->write(obj, value);
	} else {
		zend_get_std_object_handlers()->write_property(object, member, value, cache_slot);
	}

	zend_string_release(member_name);
}

static HashTable *php_http_message_object_get_debug_info(zval *object, int *is_temp)
{
	zval tmp;
	php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, object);
	HashTable *props = zend_get_std_object_handlers()->get_properties(object);
	char *ver_str, *url_str = NULL;
	size_t ver_len, url_len = 0;

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);
	*is_temp = 0;

#define UPDATE_PROP(name_str, action_with_tmp) \
	do { \
		zend_property_info *pi; \
		if ((pi = zend_hash_str_find_ptr(&obj->zo.ce->properties_info, name_str, lenof(name_str)))) { \
			action_with_tmp; \
			zend_hash_update_ind(props, pi->name, &tmp); \
		} \
	} while(0)

	UPDATE_PROP("type", ZVAL_LONG(&tmp, obj->message->type));

	ver_len = spprintf(&ver_str, 0, "%u.%u", obj->message->http.version.major, obj->message->http.version.minor);
	UPDATE_PROP("httpVersion", ZVAL_STR(&tmp, php_http_cs2zs(ver_str, ver_len)));

	switch (obj->message->type) {
		case PHP_HTTP_REQUEST:
			UPDATE_PROP("responseCode", ZVAL_LONG(&tmp, 0));
			UPDATE_PROP("responseStatus", ZVAL_EMPTY_STRING(&tmp));
			UPDATE_PROP("requestMethod", ZVAL_STRING(&tmp, STR_PTR(obj->message->http.info.request.method)));
			if (obj->message->http.info.request.url) {
				php_http_url_to_string(obj->message->http.info.request.url, &url_str, &url_len, 0);
				UPDATE_PROP("requestUrl", ZVAL_STR(&tmp, php_http_cs2zs(url_str, url_len)));
			} else {
				UPDATE_PROP("requestUrl", ZVAL_EMPTY_STRING(&tmp));
			}

			break;

		case PHP_HTTP_RESPONSE:
			UPDATE_PROP("responseCode", ZVAL_LONG(&tmp, obj->message->http.info.response.code));
			UPDATE_PROP("responseStatus", ZVAL_STRING(&tmp, STR_PTR(obj->message->http.info.response.status)));
			UPDATE_PROP("requestMethod", ZVAL_EMPTY_STRING(&tmp));
			UPDATE_PROP("requestUrl", ZVAL_EMPTY_STRING(&tmp));
			break;

		case PHP_HTTP_NONE:
		default:
			UPDATE_PROP("responseCode", ZVAL_LONG(&tmp, 0));
			UPDATE_PROP("responseStatus", ZVAL_EMPTY_STRING(&tmp));
			UPDATE_PROP("requestMethod", ZVAL_EMPTY_STRING(&tmp));
			UPDATE_PROP("requestUrl", ZVAL_EMPTY_STRING(&tmp));
			break;
	}

	UPDATE_PROP("headers",
			array_init(&tmp);
			array_copy(&obj->message->hdrs, Z_ARRVAL(tmp));
	);

	UPDATE_PROP("body",
			if (obj->body) {
				ZVAL_OBJECT(&tmp, &obj->body->zo, 1);
			} else {
				ZVAL_NULL(&tmp);
			}
	);

	UPDATE_PROP("parentMessage",
			if (obj->message->parent) {
				ZVAL_OBJECT(&tmp, &obj->parent->zo, 1);
			} else {
				ZVAL_NULL(&tmp);
			}
	);

	return props;
}

static HashTable *php_http_message_object_get_gc(zval *object, zval **table, int *n)
{
	php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, object);
	HashTable *props = Z_OBJPROP_P(object);
	uint32_t count = 2 + zend_hash_num_elements(props);
	zval *val;

	*n = 0;
	*table = obj->gc = erealloc(obj->gc, count * sizeof(zval));

	if (obj->body) {
		ZVAL_OBJ(&obj->gc[(*n)++], &obj->body->zo);
	}
	if (obj->parent) {
		ZVAL_OBJ(&obj->gc[(*n)++], &obj->parent->zo);
	}

	ZEND_HASH_FOREACH_VAL(props, val)
	{
		ZVAL_COPY_VALUE(&obj->gc[(*n)++], val);
	}
	ZEND_HASH_FOREACH_END();

	return NULL;
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
	php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|z!b", &zmessage, &greedy), invalid_arg, return);

	zend_replace_error_handling(EH_THROW, php_http_get_exception_bad_message_class_entry(), &zeh);
	if (zmessage && Z_TYPE_P(zmessage) == IS_RESOURCE) {
		php_stream *s;
		php_http_message_parser_t p;
		zend_error_handling zeh;

		zend_replace_error_handling(EH_THROW, php_http_get_exception_unexpected_val_class_entry(), &zeh);
		php_stream_from_zval(s, zmessage);
		zend_restore_error_handling(&zeh);

		if (s && php_http_message_parser_init(&p)) {
			unsigned flags = (greedy ? PHP_HTTP_MESSAGE_PARSER_GREEDY : 0);
			php_http_buffer_t buf;

			php_http_buffer_init_ex(&buf, 0x1000, PHP_HTTP_BUFFER_INIT_PREALLOC);
			if (PHP_HTTP_MESSAGE_PARSER_STATE_FAILURE == php_http_message_parser_parse_stream(&p, &buf, s, flags, &msg)) {
				if (!EG(exception)) {
					php_http_throw(bad_message, "Could not parse message from stream", NULL);
				}
			}
			php_http_buffer_dtor(&buf);
			php_http_message_parser_dtor(&p);
		}

		if (!msg && !EG(exception)) {
			php_http_throw(bad_message, "Empty message received from stream", NULL);
		}
	} else if (zmessage) {
		zend_string *zs_msg = zval_get_string(zmessage);

		msg = php_http_message_parse(NULL, zs_msg->val, zs_msg->len, greedy);

		if (!msg && !EG(exception)) {
			php_http_throw(bad_message, "Could not parse message: %.*s", MIN(25, zs_msg->len), zs_msg->val);
		}
		zend_string_release(zs_msg);
	}

	if (msg) {
		php_http_message_dtor(obj->message);
		obj->message = msg;
		if (obj->message->parent) {
			obj->parent = php_http_message_object_new_ex(obj->zo.ce, obj->message->parent);
		}
	}
	zend_restore_error_handling(&zeh);
	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getBody, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getBody)
{
	php_http_message_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (!obj->body) {
		php_http_message_object_init_body_object(obj);
	}
	if (obj->body) {
		RETVAL_OBJECT(&obj->body->zo, 1);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setBody, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, body, http\\Message\\Body, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setBody)
{
	zval *zbody;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O", &zbody, php_http_get_message_body_class_entry())) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);
		php_http_message_object_prophandler_set_body(obj, zbody);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_addBody, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, body, http\\Message\\Body, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, addBody)
{
	zval *new_body;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O", &new_body, php_http_get_message_body_class_entry())) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		php_http_message_body_object_t *new_obj = PHP_HTTP_OBJ(NULL, new_body);

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
	size_t header_len;
	zend_class_entry *header_ce = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|C!", &header_str, &header_len, &header_ce)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		zval *header;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if ((header = php_http_message_header(obj->message, header_str, header_len))) {
			if (!header_ce) {
				RETURN_ZVAL(header, 1, 0);
			} else if (instanceof_function(header_ce, php_http_header_get_class_entry())) {
				php_http_object_method_t cb;
				zval argv[2];

				ZVAL_STRINGL(&argv[0], header_str, header_len);
				ZVAL_COPY(&argv[1], header);

				object_init_ex(return_value, header_ce);
				php_http_object_method_init(&cb, return_value, ZEND_STRL("__construct"));
				php_http_object_method_call(&cb, return_value, NULL, 2, argv);
				php_http_object_method_dtor(&cb);

				zval_ptr_dtor(&argv[0]);
				zval_ptr_dtor(&argv[1]);

				return;
			} else {
				php_error_docref(NULL, E_WARNING, "Class '%s' is not as descendant of http\\Header", header_ce->name->val);
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
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

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
	size_t name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s|z!", &name_str, &name_len, &zvalue)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		char *name = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (!zvalue) {
			zend_symtable_str_del(&obj->message->hdrs, name, name_len);
		} else {
			Z_TRY_ADDREF_P(zvalue);
			zend_symtable_str_update(&obj->message->hdrs, name, name_len, zvalue);
		}
		efree(name);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setHeaders, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, headers, 1)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setHeaders)
{
	zval *new_headers = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "a/!", &new_headers)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		zend_hash_clean(&obj->message->hdrs);
		if (new_headers) {
			array_join(Z_ARRVAL_P(new_headers), &obj->message->hdrs, 0, ARRAY_JOIN_PRETTIFY|ARRAY_JOIN_STRONLY);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

static inline void php_http_message_object_add_header(php_http_message_object_t *obj, const char *name_str, size_t name_len, zval *zvalue)
{
	char *name = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);
	zend_string *hstr, *vstr;
	zval *header, tmp;

	if (Z_TYPE_P(zvalue) == IS_NULL) {
		return;
	}

	vstr = php_http_header_value_to_string(zvalue);

	if ((name_len != lenof("Set-Cookie") && strcmp(name, "Set-Cookie"))
	&&	(hstr = php_http_message_header_string(obj->message, name, name_len))) {
		char *hdr_str;
		size_t hdr_len = spprintf(&hdr_str, 0, "%s, %s", hstr->val, vstr->val);

		ZVAL_STR(&tmp, php_http_cs2zs(hdr_str, hdr_len));
		zend_symtable_str_update(&obj->message->hdrs, name, name_len, &tmp);
		zend_string_release(hstr);
		zend_string_release(vstr);
	} else if ((header = php_http_message_header(obj->message, name, name_len))) {
		convert_to_array(header);
		ZVAL_STR(&tmp, vstr);
		zend_hash_next_index_insert(Z_ARRVAL_P(header), &tmp);
	} else {
		ZVAL_STR(&tmp, vstr);
		zend_symtable_str_update(&obj->message->hdrs, name, name_len, &tmp);
	}
	efree(name);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_addHeader, 0, 0, 2)
	ZEND_ARG_INFO(0, header)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, addHeader)
{
	zval *zvalue;
	char *name_str;
	size_t name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "sz", &name_str, &name_len, &zvalue)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_http_message_object_add_header(obj, name_str, name_len, zvalue);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_addHeaders, 0, 0, 1)
	ZEND_ARG_ARRAY_INFO(0, headers, 0)
	ZEND_ARG_INFO(0, append)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, addHeaders)
{
	zval *new_headers;
	zend_bool append = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "a|b", &new_headers, &append)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (append) {
			php_http_arrkey_t key = {0};
			zval *val;

			ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(new_headers), key.h, key.key, val)
			{
				php_http_arrkey_stringify(&key, NULL);
				php_http_message_object_add_header(obj, key.key->val, key.key->len, val);
				php_http_arrkey_dtor(&key);
			}
			ZEND_HASH_FOREACH_END();
		} else {
			array_join(Z_ARRVAL_P(new_headers), &obj->message->hdrs, 0, ARRAY_JOIN_PRETTIFY|ARRAY_JOIN_STRONLY);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getType, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getType)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		RETURN_LONG(obj->message->type);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setType, 0, 0, 1)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setType)
{
	zend_long type;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "l", &type)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

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
		char *str = NULL;
		size_t len = 0;
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);
		php_http_info_to_string((php_http_info_t *) obj->message, &str, &len, "");

		RETVAL_STR(php_http_cs2zs(str, len));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setInfo, 0, 0, 1)
	ZEND_ARG_INFO(0, http_info)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setInfo)
{
	char *str;
	size_t len;
	php_http_message_object_t *obj;
	php_http_info_t inf;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &str, &len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (!php_http_info_parse(&inf, str)) {
		php_http_throw(bad_header, "Could not parse message info '%s'", str);
		return;
	}

	php_http_message_set_info(obj->message, &inf);
	php_http_info_dtor(&inf);

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getHttpVersion, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getHttpVersion)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		char *str;
		size_t len;
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_http_version_to_string(&obj->message->http.version, &str, &len, NULL, NULL);
		RETURN_STR(php_http_cs2zs(str, len));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setHttpVersion, 0, 0, 1)
	ZEND_ARG_INFO(0, http_version)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setHttpVersion)
{
	char *v_str;
	size_t v_len;
	php_http_version_t version;
	php_http_message_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &v_str, &v_len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	php_http_expect(php_http_version_parse(&version, v_str), unexpected_val, return);

	obj->message->http.version = version;

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getResponseCode, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getResponseCode)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (obj->message->type != PHP_HTTP_RESPONSE) {
			php_error_docref(NULL, E_WARNING, "http\\Message is not if type response");
			RETURN_FALSE;
		}

		RETURN_LONG(obj->message->http.info.response.code);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setResponseCode, 0, 0, 1)
	ZEND_ARG_INFO(0, response_code)
	ZEND_ARG_INFO(0, strict)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setResponseCode)
{
	zend_long code;
	zend_bool strict = 1;
	php_http_message_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "l|b", &code, &strict), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (obj->message->type != PHP_HTTP_RESPONSE) {
		php_http_throw(bad_method_call, "http\\Message is not of type response", NULL);
		return;
	}

	if (strict && (code < 100 || code > 599)) {
		php_http_throw(invalid_arg, "Invalid response code (100-599): %ld", code);
		return;
	}

	obj->message->http.info.response.code = code;
	PTR_SET(obj->message->http.info.response.status, estrdup(php_http_env_get_response_status_for_code(code)));

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getResponseStatus, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getResponseStatus)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (obj->message->type != PHP_HTTP_RESPONSE) {
			php_error_docref(NULL, E_WARNING, "http\\Message is not of type response");
		}

		if (obj->message->http.info.response.status) {
			RETURN_STRING(obj->message->http.info.response.status);
		} else {
			RETURN_EMPTY_STRING();
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setResponseStatus, 0, 0, 1)
	ZEND_ARG_INFO(0, response_status)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setResponseStatus)
{
	char *status;
	size_t status_len;
	php_http_message_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &status, &status_len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (obj->message->type != PHP_HTTP_RESPONSE) {
		php_http_throw(bad_method_call, "http\\Message is not of type response", NULL);
	}

	PTR_SET(obj->message->http.info.response.status, estrndup(status, status_len));
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getRequestMethod, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getRequestMethod)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (obj->message->type != PHP_HTTP_REQUEST) {
			php_error_docref(NULL, E_WARNING, "http\\Message is not of type request");
			RETURN_FALSE;
		}

		if (obj->message->http.info.request.method) {
			RETURN_STRING(obj->message->http.info.request.method);
		} else {
			RETURN_EMPTY_STRING();
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setRequestMethod, 0, 0, 1)
	ZEND_ARG_INFO(0, request_method)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setRequestMethod)
{
	char *method;
	size_t method_len;
	php_http_message_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &method, &method_len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (obj->message->type != PHP_HTTP_REQUEST) {
		php_http_throw(bad_method_call, "http\\Message is not of type request", NULL);
		return;
	}

	if (method_len < 1) {
		php_http_throw(invalid_arg, "Cannot set http\\Message's request method to an empty string", NULL);
		return;
	}

	PTR_SET(obj->message->http.info.request.method, estrndup(method, method_len));
	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getRequestUrl, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getRequestUrl)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (obj->message->type != PHP_HTTP_REQUEST) {
			php_error_docref(NULL, E_WARNING, "http\\Message is not of type request");
			RETURN_FALSE;
		}

		if (obj->message->http.info.request.url) {
			char *url_str;
			size_t url_len;

			php_http_url_to_string(obj->message->http.info.request.url, &url_str, &url_len, 0);
			RETURN_STR(php_http_cs2zs(url_str, url_len));
		} else {
			RETURN_EMPTY_STRING();
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_setRequestUrl, 0, 0, 1)
	ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, setRequestUrl)
{
	zval *zurl;
	php_http_url_t *url;
	php_http_message_object_t *obj;
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "z", &zurl), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());

	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (obj->message->type != PHP_HTTP_REQUEST) {
		php_http_throw(bad_method_call, "http\\Message is not of type request", NULL);
		return;
	}

	zend_replace_error_handling(EH_THROW, php_http_get_exception_bad_url_class_entry(), &zeh);
	url = php_http_url_from_zval(zurl, PHP_HTTP_URL_STDFLAGS);
	zend_restore_error_handling(&zeh);

	if (url && php_http_url_is_empty(url)) {
		php_http_url_free(&url);
		php_http_throw(invalid_arg, "Cannot set http\\Message's request url to an empty string", NULL);
	} else if (url) {
		PTR_SET(obj->message->http.info.request.url, url);
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_getParentMessage, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, getParentMessage)
{
	php_http_message_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (!obj->message->parent) {
		php_http_throw(unexpected_val, "http\\Message has not parent message", NULL);
		return;
	}

	RETVAL_OBJECT(&obj->parent->zo, 1);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage___toString, 0, 0, 0)
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_toString, 0, 0, 0)
	ZEND_ARG_INFO(0, include_parent)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, toString)
{
	zend_bool include_parent = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &include_parent)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		char *string;
		size_t length;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (include_parent) {
			php_http_message_serialize(obj->message, &string, &length);
		} else {
			php_http_message_to_string(obj->message, &string, &length);
		}
		if (string) {
			RETURN_STR(php_http_cs2zs(string, length));
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

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "r", &zstream)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		php_stream *s;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_stream_from_zval(s, zstream);
		php_http_message_to_callback(obj->message, (php_http_pass_callback_t) _php_stream_write, s);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_toCallback, 0, 0, 1)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, toCallback)
{
	php_http_pass_fcall_arg_t fcd;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "f", &fcd.fci, &fcd.fcc)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		ZVAL_COPY(&fcd.fcz, getThis());
		php_http_message_to_callback(obj->message, php_http_pass_fcall_callback, &fcd);
		zend_fcall_info_args_clear(&fcd.fci, 1);
		zval_ptr_dtor(&fcd.fcz);

		RETURN_ZVAL(&fcd.fcz, 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_serialize, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, serialize)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		char *string;
		size_t length;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		php_http_message_serialize(obj->message, &string, &length);
		RETURN_STR(php_http_cs2zs(string, length));
	}
	RETURN_EMPTY_STRING();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_unserialize, 0, 0, 1)
	ZEND_ARG_INFO(0, serialized)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, unserialize)
{
	size_t length;
	char *serialized;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &serialized, &length)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		php_http_message_t *msg;

		if (obj->message) {
			/* do not free recursively */
			php_http_message_dtor(obj->message);
			efree(obj->message);
		}
		if ((msg = php_http_message_parse(NULL, serialized, length, 1))) {
			obj->message = msg;
		} else {
			obj->message = php_http_message_init(NULL, 0, NULL);
			php_error_docref(NULL, E_ERROR, "Could not unserialize http\\Message");
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_detach, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, detach)
{
	php_http_message_object_t *obj, *new_obj;
	php_http_message_t *msg_cpy;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	msg_cpy = php_http_message_copy_ex(obj->message, NULL, 0);
	new_obj = php_http_message_object_new_ex(obj->zo.ce, msg_cpy);

	RETVAL_OBJ(&new_obj->zo);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_prepend, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, message, http\\Message, 0)
	ZEND_ARG_INFO(0, top)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, prepend)
{
	zval *prepend;
	zend_bool top = 1;
	php_http_message_t *msg[2];
	php_http_message_object_t *obj, *prepend_obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O|b", &prepend, php_http_message_class_entry, &top), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);
	prepend_obj = PHP_HTTP_OBJ(NULL, prepend);
	PHP_HTTP_MESSAGE_OBJECT_INIT(prepend_obj);

	/* safety check */
	for (msg[0] = obj->message; msg[0]; msg[0] = msg[0]->parent) {
		for (msg[1] = prepend_obj->message; msg[1]; msg[1] = msg[1]->parent) {
			if (msg[0] == msg[1]) {
				php_http_throw(unexpected_val, "Cannot prepend a message located within the same message chain", NULL);
				return;
			}
		}
	}

	php_http_message_object_prepend(getThis(), prepend, top);
	RETURN_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_reverse, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, reverse)
{
	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	php_http_message_object_reverse(getThis(), return_value);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_isMultipart, 0, 0, 0)
	ZEND_ARG_INFO(1, boundary)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, isMultipart)
{
	zval *zboundary = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &zboundary)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		char *boundary = NULL;

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		if (php_http_message_is_multipart(obj->message, zboundary ? &boundary : NULL)) {
			RETVAL_TRUE;
		} else {
			RETVAL_FALSE;
		}

		if (zboundary && boundary) {
			ZVAL_DEREF(zboundary);
			zval_dtor(zboundary);
			ZVAL_STR(zboundary, php_http_cs2zs(boundary, strlen(boundary)));
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_splitMultipartBody, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, splitMultipartBody)
{
	php_http_message_object_t *obj;
	php_http_message_t *msg;
	char *boundary = NULL;

	php_http_expect(SUCCESS == zend_parse_parameters_none(), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

	if (!php_http_message_is_multipart(obj->message, &boundary)) {
		php_http_throw(bad_method_call, "http\\Message is not a multipart message", NULL);
		return;
	}

	php_http_expect(msg = php_http_message_body_split(obj->message->body, boundary), bad_message, return);

	PTR_FREE(boundary);

	RETURN_OBJ(&php_http_message_object_new_ex(obj->zo.ce, msg)->zo);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_count, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, count)
{
	zend_long count_mode = -1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &count_mode)) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_OBJECT_INIT(obj);

		RETURN_LONG(php_http_message_count(obj->message));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_rewind, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, rewind)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *zobj = getThis();
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		if (!Z_ISUNDEF(obj->iterator)) {
			zval_ptr_dtor(&obj->iterator);
		}
		ZVAL_COPY(&obj->iterator, zobj);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_valid, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, valid)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		RETURN_BOOL(!Z_ISUNDEF(obj->iterator));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_next, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, next)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		if (!Z_ISUNDEF(obj->iterator)) {
			php_http_message_object_t *itr = PHP_HTTP_OBJ(NULL, &obj->iterator);

			if (itr->parent) {
				zval tmp;

				ZVAL_OBJECT(&tmp, &itr->parent->zo, 1);
				zval_ptr_dtor(&obj->iterator);
				obj->iterator = tmp;
			} else {
				zval_ptr_dtor(&obj->iterator);
				ZVAL_UNDEF(&obj->iterator);
			}
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_key, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, key)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		RETURN_LONG(Z_ISUNDEF(obj->iterator) ? 0 : Z_OBJ_HANDLE(obj->iterator));
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessage_current, 0, 0, 0)
ZEND_END_ARG_INFO();
static PHP_METHOD(HttpMessage, current)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		if (!Z_ISUNDEF(obj->iterator)) {
			RETURN_ZVAL(&obj->iterator, 1, 0);
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

PHP_MINIT_FUNCTION(http_message)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http", "Message", php_http_message_methods);
	php_http_message_class_entry = zend_register_internal_class(&ce);
	php_http_message_class_entry->create_object = php_http_message_object_new;
	memcpy(&php_http_message_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_message_object_handlers.offset = XtOffsetOf(php_http_message_object_t, zo);
	php_http_message_object_handlers.clone_obj = php_http_message_object_clone;
	php_http_message_object_handlers.free_obj = php_http_message_object_free;
	php_http_message_object_handlers.read_property = php_http_message_object_read_prop;
	php_http_message_object_handlers.write_property = php_http_message_object_write_prop;
	php_http_message_object_handlers.get_debug_info = php_http_message_object_get_debug_info;
	php_http_message_object_handlers.get_property_ptr_ptr = NULL;
	php_http_message_object_handlers.get_gc = php_http_message_object_get_gc;

	zend_class_implements(php_http_message_class_entry, 3, spl_ce_Countable, zend_ce_serializable, zend_ce_iterator);

	zend_hash_init(&php_http_message_object_prophandlers, 9, NULL, php_http_message_object_prophandler_hash_dtor, 1);
	zend_declare_property_long(php_http_message_class_entry, ZEND_STRL("type"), PHP_HTTP_NONE, ZEND_ACC_PROTECTED);
	php_http_message_object_add_prophandler(ZEND_STRL("type"), php_http_message_object_prophandler_get_type, php_http_message_object_prophandler_set_type);
	zend_declare_property_null(php_http_message_class_entry, ZEND_STRL("body"), ZEND_ACC_PROTECTED);
	php_http_message_object_add_prophandler(ZEND_STRL("body"), php_http_message_object_prophandler_get_body, php_http_message_object_prophandler_set_body);
	zend_declare_property_string(php_http_message_class_entry, ZEND_STRL("requestMethod"), "", ZEND_ACC_PROTECTED);
	php_http_message_object_add_prophandler(ZEND_STRL("requestMethod"), php_http_message_object_prophandler_get_request_method, php_http_message_object_prophandler_set_request_method);
	zend_declare_property_string(php_http_message_class_entry, ZEND_STRL("requestUrl"), "", ZEND_ACC_PROTECTED);
	php_http_message_object_add_prophandler(ZEND_STRL("requestUrl"), php_http_message_object_prophandler_get_request_url, php_http_message_object_prophandler_set_request_url);
	zend_declare_property_string(php_http_message_class_entry, ZEND_STRL("responseStatus"), "", ZEND_ACC_PROTECTED);
	php_http_message_object_add_prophandler(ZEND_STRL("responseStatus"), php_http_message_object_prophandler_get_response_status, php_http_message_object_prophandler_set_response_status);
	zend_declare_property_long(php_http_message_class_entry, ZEND_STRL("responseCode"), 0, ZEND_ACC_PROTECTED);
	php_http_message_object_add_prophandler(ZEND_STRL("responseCode"), php_http_message_object_prophandler_get_response_code, php_http_message_object_prophandler_set_response_code);
	zend_declare_property_null(php_http_message_class_entry, ZEND_STRL("httpVersion"), ZEND_ACC_PROTECTED);
	php_http_message_object_add_prophandler(ZEND_STRL("httpVersion"), php_http_message_object_prophandler_get_http_version, php_http_message_object_prophandler_set_http_version);
	zend_declare_property_null(php_http_message_class_entry, ZEND_STRL("headers"), ZEND_ACC_PROTECTED);
	php_http_message_object_add_prophandler(ZEND_STRL("headers"), php_http_message_object_prophandler_get_headers, php_http_message_object_prophandler_set_headers);
	zend_declare_property_null(php_http_message_class_entry, ZEND_STRL("parentMessage"), ZEND_ACC_PROTECTED);
	php_http_message_object_add_prophandler(ZEND_STRL("parentMessage"), php_http_message_object_prophandler_get_parent_message, php_http_message_object_prophandler_set_parent_message);

	zend_declare_class_constant_long(php_http_message_class_entry, ZEND_STRL("TYPE_NONE"), PHP_HTTP_NONE);
	zend_declare_class_constant_long(php_http_message_class_entry, ZEND_STRL("TYPE_REQUEST"), PHP_HTTP_REQUEST);
	zend_declare_class_constant_long(php_http_message_class_entry, ZEND_STRL("TYPE_RESPONSE"), PHP_HTTP_RESPONSE);

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
