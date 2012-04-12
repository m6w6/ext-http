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

PHP_HTTP_API zend_bool php_http_message_info_callback(php_http_message_t **message, HashTable **headers, php_http_info_t *info TSRMLS_DC)
{
	php_http_message_t *old = *message;

	/* advance message */
	if (!old || old->type || zend_hash_num_elements(&old->hdrs) || PHP_HTTP_BUFFER_LEN(old)) {
		(*message) = php_http_message_init(NULL, 0 TSRMLS_CC);
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

PHP_HTTP_API php_http_message_t *php_http_message_init(php_http_message_t *message, php_http_message_type_t type TSRMLS_DC)
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
	php_http_message_body_init(&message->body, NULL TSRMLS_CC);

	return message;
}

PHP_HTTP_API php_http_message_t *php_http_message_init_env(php_http_message_t *message, php_http_message_type_t type TSRMLS_DC)
{
	int free_msg = !message;
	zval *sval, tval;
	php_http_message_body_t *mbody;
	
	message = php_http_message_init(message, type TSRMLS_CC);
	
	switch (type) {
		case PHP_HTTP_REQUEST:
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

			if ((mbody = php_http_env_get_request_body(TSRMLS_C))) {
				php_http_message_body_dtor(&message->body);
				php_http_message_body_copy(mbody, &message->body, 0);
			}
			break;
			
		case PHP_HTTP_RESPONSE:
			if (!SG(sapi_headers).http_status_line || !php_http_info_parse((php_http_info_t *) &message->http, SG(sapi_headers).http_status_line TSRMLS_CC)) {
				if (!(message->http.info.response.code = SG(sapi_headers).http_response_code)) {
					message->http.info.response.code = 200;
				}
				message->http.info.response.status = estrdup(php_http_env_get_response_status_for_code(message->http.info.response.code));
			}
			
			php_http_env_get_response_headers(&message->hdrs TSRMLS_CC);

			if (php_output_get_level(TSRMLS_C)) {
				if (php_output_get_status(TSRMLS_C) & PHP_OUTPUT_SENT) {
					php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "Could not fetch response body, output has already been sent at %s:%d", php_output_get_start_filename(TSRMLS_C), php_output_get_start_lineno(TSRMLS_C));
					goto error;
				} else if (SUCCESS != php_output_get_contents(&tval TSRMLS_CC)) {
					php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "Could not fetch response body");
					goto error;
				} else {
					php_http_message_body_append(&message->body, Z_STRVAL(tval), Z_STRLEN(tval));
					zval_dtor(&tval);
				}
			}
			break;
			
		default:
		error:
			if (free_msg) {
				php_http_message_free(&message);
			} else {
				message = NULL;
			}
			break;
	}
	
	return message;
}

PHP_HTTP_API php_http_message_t *php_http_message_parse(php_http_message_t *msg, const char *str, size_t len TSRMLS_DC)
{
	php_http_message_parser_t p;
	php_http_buffer_t buf;
	int free_msg;

	php_http_buffer_from_string_ex(&buf, str, len);
	php_http_message_parser_init(&p TSRMLS_CC);

	if ((free_msg = !msg)) {
		msg = php_http_message_init(NULL, 0 TSRMLS_CC);
	}

	if (FAILURE == php_http_message_parser_parse(&p, &buf, PHP_HTTP_MESSAGE_PARSER_CLEANUP, &msg)) {
		if (free_msg) {
			php_http_message_free(&msg);
		}
		msg = NULL;
	}

	php_http_message_parser_dtor(&p);
	php_http_buffer_dtor(&buf);

	return msg;
}

PHP_HTTP_API zval *php_http_message_header(php_http_message_t *msg, char *key_str, size_t key_len, int join)
{
	zval *ret = NULL, **header;
	char *key = php_http_pretty_key(estrndup(key_str, key_len), key_len, 1, 1);

	if (SUCCESS == zend_symtable_find(&msg->hdrs, key, key_len + 1, (void *) &header)) {
		if (join && Z_TYPE_PP(header) == IS_ARRAY) {
			zval *header_str, **val;
			HashPosition pos;
			php_http_buffer_t str;
			TSRMLS_FETCH_FROM_CTX(msg->ts);

			php_http_buffer_init(&str);
			MAKE_STD_ZVAL(header_str);
			FOREACH_VAL(pos, *header, val) {
				zval *strval = php_http_ztyp(IS_STRING, *val);
				php_http_buffer_appendf(&str, PHP_HTTP_BUFFER_LEN(&str) ? ", %s":"%s", Z_STRVAL_P(strval));
				zval_ptr_dtor(&strval);
			}
			php_http_buffer_fix(&str);
			ZVAL_STRINGL(header_str, PHP_HTTP_BUFFER_VAL(&str), PHP_HTTP_BUFFER_LEN(&str), 0);
			ret = header_str;
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

static inline void message_headers(php_http_message_t *msg, php_http_buffer_t *str)
{
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	HashPosition pos1;
	zval **header, *h;
	size_t size;
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

	if ((size = php_http_message_body_size(&msg->body))) {
		MAKE_STD_ZVAL(h);
		ZVAL_LONG(h, size);
		zend_hash_update(&msg->hdrs, "Content-Length", sizeof("Content-Length"), &h, sizeof(zval *), NULL);

		if (msg->body.boundary) {
			char *str;
			size_t len;

			if (!(h = php_http_message_header(msg, ZEND_STRL("Content-Type"), 1))) {
				len = spprintf(&str, 0, "multipart/form-data; boundary=\"%s\"", msg->body.boundary);
				MAKE_STD_ZVAL(h);
				ZVAL_STRINGL(h, str, len, 0);
				zend_hash_update(&msg->hdrs, "Content-Type", sizeof("Content-Type"), &h, sizeof(zval *), NULL);
			} else if (!php_http_match(Z_STRVAL_P(h), "boundary=", PHP_HTTP_MATCH_WORD)) {
				zval_dtor(h);
				Z_STRLEN_P(h) = spprintf(&Z_STRVAL_P(h), 0, "%s; boundary=\"%s\"", Z_STRVAL_P(h), msg->body.boundary);
				zend_hash_update(&msg->hdrs, "Content-Type", sizeof("Content-Type"), &h, sizeof(zval *), NULL);
			} else {
				zval_ptr_dtor(&h);
			}
		}
	}

	FOREACH_HASH_KEYVAL(pos1, &msg->hdrs, key, header) {
		if (key.type == HASH_KEY_IS_STRING) {
			HashPosition pos2;
			zval **single_header;

			switch (Z_TYPE_PP(header)) {
				case IS_BOOL:
					php_http_buffer_appendf(str, "%s: %s" PHP_HTTP_CRLF, key.str, Z_BVAL_PP(header)?"true":"false");
					break;
					
				case IS_LONG:
					php_http_buffer_appendf(str, "%s: %ld" PHP_HTTP_CRLF, key.str, Z_LVAL_PP(header));
					break;
					
				case IS_DOUBLE:
					php_http_buffer_appendf(str, "%s: %F" PHP_HTTP_CRLF, key.str, Z_DVAL_PP(header));
					break;
					
				case IS_STRING:
					if (Z_STRVAL_PP(header)[Z_STRLEN_PP(header)-1] == '\r') fprintf(stderr, "DOH!\n");
					php_http_buffer_appendf(str, "%s: %s" PHP_HTTP_CRLF, key.str, Z_STRVAL_PP(header));
					break;

				case IS_ARRAY:
					FOREACH_VAL(pos2, *header, single_header) {
						switch (Z_TYPE_PP(single_header)) {
							case IS_BOOL:
								php_http_buffer_appendf(str, "%s: %s" PHP_HTTP_CRLF, key.str, Z_BVAL_PP(single_header)?"true":"false");
								break;
								
							case IS_LONG:
								php_http_buffer_appendf(str, "%s: %ld" PHP_HTTP_CRLF, key.str, Z_LVAL_PP(single_header));
								break;
								
							case IS_DOUBLE:
								php_http_buffer_appendf(str, "%s: %F" PHP_HTTP_CRLF, key.str, Z_DVAL_PP(single_header));
								break;
								
							case IS_STRING:
								php_http_buffer_appendf(str, "%s: %s" PHP_HTTP_CRLF, key.str, Z_STRVAL_PP(single_header));
								break;
						}
					}
					break;
			}
		}
	}
}

PHP_HTTP_API void php_http_message_to_callback(php_http_message_t *msg, php_http_pass_callback_t cb, void *cb_arg)
{
	php_http_buffer_t str;

	php_http_buffer_init_ex(&str, 0x1000, 0);
	message_headers(msg, &str);
	cb(cb_arg, PHP_HTTP_BUFFER_VAL(&str), PHP_HTTP_BUFFER_LEN(&str));
	php_http_buffer_dtor(&str);

	if (php_http_message_body_size(&msg->body)) {
		cb(cb_arg, ZEND_STRL(PHP_HTTP_CRLF));
		php_http_message_body_to_callback(&msg->body, cb, cb_arg, 0, 0);
	}
}

PHP_HTTP_API void php_http_message_to_string(php_http_message_t *msg, char **string, size_t *length)
{
	php_http_buffer_t str;
	char *data;

	php_http_buffer_init_ex(&str, 0x1000, 0);
	message_headers(msg, &str);
	if (php_http_message_body_size(&msg->body)) {
		php_http_buffer_appends(&str, PHP_HTTP_CRLF);
		php_http_message_body_to_callback(&msg->body, (php_http_pass_callback_t) php_http_buffer_append, &str, 0, 0);
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

PHP_HTTP_API php_http_message_t *php_http_message_copy_ex(php_http_message_t *from, php_http_message_t *to, zend_bool parents)
{
	php_http_message_t *temp, *copy = NULL;
	php_http_info_t info;
	TSRMLS_FETCH_FROM_CTX(from->ts);
	
	if (from) {
		info.type = from->type;
		info.http = from->http;
		
		copy = temp = php_http_message_init(to, 0 TSRMLS_CC);
		php_http_message_set_info(temp, &info);
		zend_hash_copy(&temp->hdrs, &from->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		php_http_message_body_copy(&from->body, &temp->body, 1);
	
		if (parents) while (from->parent) {
			info.type = from->parent->type;
			info.http = from->parent->http;
		
			temp->parent = php_http_message_init(NULL, 0 TSRMLS_CC);
			php_http_message_set_info(temp->parent, &info);
			zend_hash_copy(&temp->parent->hdrs, &from->parent->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
			php_http_message_body_copy(&from->parent->body, &temp->parent->body, 1);
		
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
		php_http_message_body_dtor(&message->body);
		
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

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpMessage, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpMessage, method, 0)
#define PHP_HTTP_MESSAGE_ME(method, visibility)	PHP_ME(HttpMessage, method, PHP_HTTP_ARGS(HttpMessage, method), visibility)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(message, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getBody);
PHP_HTTP_BEGIN_ARGS(setBody, 1)
	PHP_HTTP_ARG_VAL(body, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addBody, 1)
	PHP_HTTP_ARG_VAL(body, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getHeader, 1)
	PHP_HTTP_ARG_VAL(header, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(setHeader, 1)
	PHP_HTTP_ARG_VAL(header, 0)
	PHP_HTTP_ARG_VAL(value, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addHeader, 2)
	PHP_HTTP_ARG_VAL(header, 0)
	PHP_HTTP_ARG_VAL(value, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getHeaders);
PHP_HTTP_BEGIN_ARGS(setHeaders, 1)
	PHP_HTTP_ARG_VAL(headers, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addHeaders, 1)
	PHP_HTTP_ARG_VAL(headers, 0)
	PHP_HTTP_ARG_VAL(append, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getType);
PHP_HTTP_BEGIN_ARGS(setType, 1)
	PHP_HTTP_ARG_VAL(type, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getInfo);
PHP_HTTP_BEGIN_ARGS(setInfo, 1)
	PHP_HTTP_ARG_VAL(http_info, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseCode);
PHP_HTTP_BEGIN_ARGS(setResponseCode, 1)
	PHP_HTTP_ARG_VAL(response_code, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseStatus);
PHP_HTTP_BEGIN_ARGS(setResponseStatus, 1)
	PHP_HTTP_ARG_VAL(response_status, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getRequestMethod);
PHP_HTTP_BEGIN_ARGS(setRequestMethod, 1)
	PHP_HTTP_ARG_VAL(request_method, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getRequestUrl);
PHP_HTTP_BEGIN_ARGS(setRequestUrl, 1)
	PHP_HTTP_ARG_VAL(url, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getHttpVersion);
PHP_HTTP_BEGIN_ARGS(setHttpVersion, 1)
	PHP_HTTP_ARG_VAL(http_version, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getParentMessage);
PHP_HTTP_EMPTY_ARGS(__toString);
PHP_HTTP_BEGIN_ARGS(toString, 0)
	PHP_HTTP_ARG_VAL(include_parent, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(toCallback, 1)
	PHP_HTTP_ARG_VAL(callback, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(toStream, 1)
	PHP_HTTP_ARG_VAL(stream, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(count);

PHP_HTTP_EMPTY_ARGS(serialize);
PHP_HTTP_BEGIN_ARGS(unserialize, 1)
	PHP_HTTP_ARG_VAL(serialized, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(rewind);
PHP_HTTP_EMPTY_ARGS(valid);
PHP_HTTP_EMPTY_ARGS(key);
PHP_HTTP_EMPTY_ARGS(current);
PHP_HTTP_EMPTY_ARGS(next);

PHP_HTTP_EMPTY_ARGS(detach);
PHP_HTTP_BEGIN_ARGS(prepend, 1)
	PHP_HTTP_ARG_OBJ(http\\Message, message, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_EMPTY_ARGS(reverse);

PHP_HTTP_BEGIN_ARGS(isMultipart, 0)
	PHP_HTTP_ARG_VAL(boundary, 1)
PHP_HTTP_END_ARGS;
PHP_HTTP_EMPTY_ARGS(splitMultipartBody);

static zval *php_http_message_object_read_prop(zval *object, zval *member, int type, const zend_literal *literal_key TSRMLS_DC);
static void php_http_message_object_write_prop(zval *object, zval *member, zval *value, const zend_literal *literal_key TSRMLS_DC);
static zval **php_http_message_object_get_prop_ptr(zval *object, zval *member, const zend_literal *literal_key TSRMLS_DC);
static HashTable *php_http_message_object_get_props(zval *object TSRMLS_DC);

static zend_class_entry *php_http_message_class_entry;

zend_class_entry *php_http_message_get_class_entry(void)
{
	return php_http_message_class_entry;
}

static zend_function_entry php_http_message_method_entry[] = {
	PHP_HTTP_MESSAGE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_MESSAGE_ME(getBody, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setBody, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(addBody, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getHeader, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setHeader, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(addHeader, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getHeaders, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setHeaders, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(addHeaders, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getType, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setType, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getInfo, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setInfo, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getResponseCode, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setResponseCode, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getResponseStatus, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setResponseStatus, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getRequestMethod, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setRequestMethod, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getRequestUrl, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setRequestUrl, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getHttpVersion, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(setHttpVersion, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(getParentMessage, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(toString, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(toCallback, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(toStream, ZEND_ACC_PUBLIC)

	/* implements Countable */
	PHP_HTTP_MESSAGE_ME(count, ZEND_ACC_PUBLIC)

	/* implements Serializable */
	PHP_HTTP_MESSAGE_ME(serialize, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(unserialize, ZEND_ACC_PUBLIC)

	/* implements Iterator */
	PHP_HTTP_MESSAGE_ME(rewind, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(valid, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(current, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(key, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(next, ZEND_ACC_PUBLIC)

	ZEND_MALIAS(HttpMessage, __toString, toString, PHP_HTTP_ARGS(HttpMessage, __toString), ZEND_ACC_PUBLIC)

	PHP_HTTP_MESSAGE_ME(detach, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(prepend, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(reverse, ZEND_ACC_PUBLIC)

	PHP_HTTP_MESSAGE_ME(isMultipart, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_ME(splitMultipartBody, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};
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
/*
static int php_http_message_object_has_prophandler(const char *prop_str, size_t prop_len) {
	return zend_hash_exists(&php_http_message_object_prophandlers, prop_str, prop_len + 1);
}
*/
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
	if (obj->body.handle) {
		RETVAL_OBJVAL(obj->body, 1);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_body(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	php_http_message_object_set_body(obj, value TSRMLS_CC);
}
static void php_http_message_object_prophandler_get_parent_message(php_http_message_object_t *obj, zval *return_value TSRMLS_DC) {
	if (obj->message->parent) {
		RETVAL_OBJVAL(obj->parent, 1);
	} else {
		RETVAL_NULL();
	}
}
static void php_http_message_object_prophandler_set_parent_message(php_http_message_object_t *obj, zval *value TSRMLS_DC) {
	if (Z_TYPE_P(value) == IS_OBJECT && instanceof_function(Z_OBJCE_P(value), php_http_message_class_entry TSRMLS_CC)) {
		if (obj->message->parent) {
			zend_objects_store_del_ref_by_handle(obj->parent.handle TSRMLS_CC);
		}
		Z_OBJ_ADDREF_P(value);
		obj->parent = Z_OBJVAL_P(value);
	}
}

PHP_MINIT_FUNCTION(http_message)
{
	PHP_HTTP_REGISTER_CLASS(http, Message, http_message, php_http_object_get_class_entry(), 0);
	php_http_message_class_entry->create_object = php_http_message_object_new;
	memcpy(&php_http_message_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_message_object_handlers.clone_obj = php_http_message_object_clone;
	php_http_message_object_handlers.read_property = php_http_message_object_read_prop;
	php_http_message_object_handlers.write_property = php_http_message_object_write_prop;
	php_http_message_object_handlers.get_properties = php_http_message_object_get_props;
	php_http_message_object_handlers.get_property_ptr_ptr = php_http_message_object_get_prop_ptr;

	zend_class_implements(php_http_message_class_entry TSRMLS_CC, 3, spl_ce_Countable, zend_ce_serializable, zend_ce_iterator);

	zend_hash_init(&php_http_message_object_prophandlers, 9, NULL, NULL, 1);
	zend_declare_property_long(php_http_message_class_entry, ZEND_STRL("type"), PHP_HTTP_NONE, ZEND_ACC_PROTECTED TSRMLS_CC);
	php_http_message_object_add_prophandler(ZEND_STRL("type"), php_http_message_object_prophandler_get_type, php_http_message_object_prophandler_set_type);
	zend_declare_property_string(php_http_message_class_entry, ZEND_STRL("body"), "", ZEND_ACC_PROTECTED TSRMLS_CC);
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

void php_http_message_object_reverse(zval *this_ptr, zval *return_value TSRMLS_DC)
{
	int i = 0;
	php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	/* count */
	php_http_message_count(i, obj->message);

	if (i > 1) {
		zend_object_value *ovalues = NULL;
		php_http_message_object_t **objects = NULL;
		int last = i - 1;

		objects = ecalloc(i, sizeof(**objects));
		ovalues = ecalloc(i, sizeof(*ovalues));

		/* we are the first message */
		objects[0] = obj;
		ovalues[0] = getThis()->value.obj;

		/* fetch parents */
		for (i = 1; obj->parent.handle; ++i) {
			ovalues[i] = obj->parent;
			objects[i] = obj = zend_object_store_get_object_by_handle(obj->parent.handle TSRMLS_CC);
		}

		/* reorder parents */
		for (last = --i; i; --i) {
			objects[i]->message->parent = objects[i-1]->message;
			objects[i]->parent = ovalues[i-1];
		}

		objects[0]->message->parent = NULL;
		objects[0]->parent.handle = 0;
		objects[0]->parent.handlers = NULL;

		/* add ref, because we previously have not been a parent message */
		Z_OBJ_ADDREF_P(getThis());
		RETVAL_OBJVAL(ovalues[last], 1);

		efree(objects);
		efree(ovalues);
	} else {
		RETURN_ZVAL(getThis(), 1, 0);
	}
}

void php_http_message_object_prepend(zval *this_ptr, zval *prepend, zend_bool top TSRMLS_DC)
{
	zval m;
	php_http_message_t *save_parent_msg = NULL;
	zend_object_value save_parent_obj = {0, NULL};
	php_http_message_object_t *obj = zend_object_store_get_object(this_ptr TSRMLS_CC);
	php_http_message_object_t *prepend_obj = zend_object_store_get_object(prepend TSRMLS_CC);

	INIT_PZVAL(&m);
	m.type = IS_OBJECT;

	if (!top) {
		save_parent_obj = obj->parent;
		save_parent_msg = obj->message->parent;
	} else {
		/* iterate to the most parent object */
		while (obj->parent.handle) {
			m.value.obj = obj->parent;
			obj = zend_object_store_get_object(&m TSRMLS_CC);
		}
	}

	/* prepend */
	obj->parent = prepend->value.obj;
	obj->message->parent = prepend_obj->message;

	/* add ref */
	zend_objects_store_add_ref(prepend TSRMLS_CC);
	while (prepend_obj->parent.handle) {
		m.value.obj = prepend_obj->parent;
		zend_objects_store_add_ref(&m TSRMLS_CC);
		prepend_obj = zend_object_store_get_object(&m TSRMLS_CC);
	}

	if (!top) {
		prepend_obj->parent = save_parent_obj;
		prepend_obj->message->parent = save_parent_msg;
	}
}

STATUS php_http_message_object_set_body(php_http_message_object_t *msg_obj, zval *zbody TSRMLS_DC)
{
	zval *tmp;
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
			if (SUCCESS != php_http_new(&ov, php_http_message_body_get_class_entry(), php_http_message_body_object_new_ex, NULL, body, NULL TSRMLS_CC)) {
				php_http_message_body_free(&body);
				return FAILURE;
			}
			MAKE_STD_ZVAL(zbody);
			ZVAL_OBJVAL(zbody, ov, 0);
			break;

		case IS_OBJECT:
			if (instanceof_function(Z_OBJCE_P(zbody), php_http_message_body_get_class_entry() TSRMLS_CC)) {
				Z_OBJ_ADDREF_P(zbody);
				break;
			}
			/* no break */

		default:
			tmp = php_http_ztyp(IS_STRING, zbody);
			s = php_stream_temp_new();
			php_stream_write(s, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
			zval_ptr_dtor(&tmp);
			goto is_resource;

	}

	body_obj = zend_object_store_get_object(zbody TSRMLS_CC);

	if (msg_obj->body.handle) {
		zend_objects_store_del_ref_by_handle(msg_obj->body.handle TSRMLS_CC);
		php_http_message_body_dtor(&msg_obj->message->body);
	}

	php_http_message_body_copy(body_obj->body, &msg_obj->message->body, 0);
	msg_obj->body = Z_OBJVAL_P(zbody);

	return SUCCESS;
}

zend_object_value php_http_message_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_message_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_message_object_new_ex(zend_class_entry *ce, php_http_message_t *msg, php_http_message_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
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
			o->parent = php_http_message_object_new_ex(ce, msg->parent, NULL TSRMLS_CC);
		}
		o->body = php_http_message_body_object_new_ex(php_http_message_body_get_class_entry(), php_http_message_body_copy(&msg->body, NULL, 0), NULL TSRMLS_CC);
	}

	ov.handle = zend_objects_store_put((zend_object *) o, NULL, php_http_message_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_message_object_handlers;

	return ov;
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
	if (o->parent.handle) {
		zend_objects_store_del_ref_by_handle(o->parent.handle TSRMLS_CC);
	}
	if (o->body.handle) {
		zend_objects_store_del_ref_by_handle(o->body.handle TSRMLS_CC);
	}
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}


static zval **php_http_message_object_get_prop_ptr(zval *object, zval *member, const zend_literal *literal_key TSRMLS_DC)
{
	php_http_message_object_prophandler_t *handler;
	zval *copy = php_http_ztyp(IS_STRING, member);

	if (SUCCESS == php_http_message_object_get_prophandler(Z_STRVAL_P(copy), Z_STRLEN_P(copy), &handler)) {
		zval_ptr_dtor(&copy);
		return &php_http_property_proxy_init(NULL, object, member, NULL TSRMLS_CC)->myself;
	}
	zval_ptr_dtor(&copy);

	return zend_get_std_object_handlers()->get_property_ptr_ptr(object, member, literal_key TSRMLS_CC);
}

static zval *php_http_message_object_read_prop(zval *object, zval *member, int type, const zend_literal *literal_key TSRMLS_DC)
{
	php_http_message_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);
	php_http_message_object_prophandler_t *handler;
	zval *return_value, *copy = php_http_ztyp(IS_STRING, member);

	if (SUCCESS == php_http_message_object_get_prophandler(Z_STRVAL_P(copy), Z_STRLEN_P(copy), &handler)) {
		if (type == BP_VAR_R) {
			ALLOC_ZVAL(return_value);
			Z_SET_REFCOUNT_P(return_value, 0);
			Z_UNSET_ISREF_P(return_value);

			handler->read(obj, return_value TSRMLS_CC);
		} else {
			zend_error(E_ERROR, "Cannot access HttpMessage properties by reference or array key/index");
			return_value = NULL;
		}
	} else {
		return_value = zend_get_std_object_handlers()->read_property(object, member, type, literal_key TSRMLS_CC);
	}

	zval_ptr_dtor(&copy);

	return return_value;
}

static void php_http_message_object_write_prop(zval *object, zval *member, zval *value, const zend_literal *literal_key TSRMLS_DC)
{
	php_http_message_object_t *obj = zend_object_store_get_object(object TSRMLS_CC);
	php_http_message_object_prophandler_t *handler;
	zval *copy = php_http_ztyp(IS_STRING, member);

	if (SUCCESS == php_http_message_object_get_prophandler(Z_STRVAL_P(copy), Z_STRLEN_P(copy), &handler)) {
		handler->write(obj, value TSRMLS_CC);
	} else {
		zend_get_std_object_handlers()->write_property(object, member, value, literal_key TSRMLS_CC);
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

	INIT_PZVAL_ARRAY(&array, props);

#define ASSOC_PROP(array, ptype, name, val) \
	{ \
		char *m_prop_name; \
		int m_prop_len; \
		zend_mangle_property_name(&m_prop_name, &m_prop_len, "*", 1, name, lenof(name), 0); \
		add_assoc_ ##ptype## _ex(&array, m_prop_name, sizeof(name)+3, val); \
		efree(m_prop_name); \
	}
#define ASSOC_STRING(array, name, val) ASSOC_STRINGL(array, name, val, strlen(val))
#define ASSOC_STRINGL(array, name, val, len) ASSOC_STRINGL_EX(array, name, val, len, 1)
#define ASSOC_STRINGL_EX(array, name, val, len, cpy) \
	{ \
		char *m_prop_name; \
		int m_prop_len; \
		zend_mangle_property_name(&m_prop_name, &m_prop_len, "*", 1, name, lenof(name), 0); \
		add_assoc_stringl_ex(&array, m_prop_name, sizeof(name)+3, val, len, cpy); \
		efree(m_prop_name); \
	}

	ASSOC_PROP(array, long, "type", msg->type);
	ASSOC_STRINGL_EX(array, "httpVersion", version, spprintf(&version, 0, "%u.%u", msg->http.version.major, msg->http.version.minor), 0);

	switch (msg->type) {
		case PHP_HTTP_REQUEST:
			ASSOC_PROP(array, long, "responseCode", 0);
			ASSOC_STRINGL(array, "responseStatus", "", 0);
			ASSOC_STRING(array, "requestMethod", STR_PTR(msg->http.info.request.method));
			ASSOC_STRING(array, "requestUrl", STR_PTR(msg->http.info.request.url));
			break;

		case PHP_HTTP_RESPONSE:
			ASSOC_PROP(array, long, "responseCode", msg->http.info.response.code);
			ASSOC_STRING(array, "responseStatus", STR_PTR(msg->http.info.response.status));
			ASSOC_STRINGL(array, "requestMethod", "", 0);
			ASSOC_STRINGL(array, "requestUrl", "", 0);
			break;

		case PHP_HTTP_NONE:
		default:
			ASSOC_PROP(array, long, "responseCode", 0);
			ASSOC_STRINGL(array, "responseStatus", "", 0);
			ASSOC_STRINGL(array, "requestMethod", "", 0);
			ASSOC_STRINGL(array, "requestUrl", "", 0);
			break;
	}

	MAKE_STD_ZVAL(headers);
	array_init(headers);
	zend_hash_copy(Z_ARRVAL_P(headers), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	ASSOC_PROP(array, zval, "headers", headers);

	MAKE_STD_ZVAL(body);
	if (!obj->body.handle) {
		php_http_new(&obj->body, php_http_message_body_get_class_entry(), (php_http_new_t) php_http_message_body_object_new_ex, NULL, (void *) php_http_message_body_copy(&obj->message->body, NULL, 0), NULL TSRMLS_CC);
	}
	ZVAL_OBJVAL(body, obj->body, 1);
	ASSOC_PROP(array, zval, "body", body);

	MAKE_STD_ZVAL(parent);
	if (msg->parent) {
		ZVAL_OBJVAL(parent, obj->parent, 1);
	} else {
		ZVAL_NULL(parent);
	}
	ASSOC_PROP(array, zval, "parentMessage", parent);

	return props;
}

/* PHP */

PHP_METHOD(HttpMessage, __construct)
{
	zval *zmessage = NULL;
	php_http_message_t *msg = NULL;
	php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z!", &zmessage) && zmessage) {
			if (Z_TYPE_P(zmessage) == IS_RESOURCE) {
				php_stream *s;
				php_http_message_parser_t p;

				php_stream_from_zval(s, &zmessage);
				if (s && php_http_message_parser_init(&p TSRMLS_CC)) {
					php_http_message_parser_parse_stream(&p, s, &msg);
					php_http_message_parser_dtor(&p);
				}
			} else {
				zmessage = php_http_ztyp(IS_STRING, zmessage);
				msg = php_http_message_parse(NULL, Z_STRVAL_P(zmessage), Z_STRLEN_P(zmessage) TSRMLS_CC);
				zval_ptr_dtor(&zmessage);
			}

			if (msg) {
				php_http_message_dtor(obj->message);
				obj->message = msg;
				if (obj->message->parent) {
					obj->parent = php_http_message_object_new_ex(Z_OBJCE_P(getThis()), obj->message->parent, NULL TSRMLS_CC);
				}
			} else {
				php_http_error(HE_THROW, PHP_HTTP_E_MESSAGE, "could not parse message: %.*s", 25, Z_STRVAL_P(zmessage));
			}
		}
		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}
	} end_error_handling();

}

PHP_METHOD(HttpMessage, getBody)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			if (!obj->message) {
				obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
			}

			if (obj->body.handle || SUCCESS == php_http_new(&obj->body, php_http_message_body_get_class_entry(), (php_http_new_t) php_http_message_body_object_new_ex, NULL, (void *) php_http_message_body_copy(&obj->message->body, NULL, 0), NULL TSRMLS_CC)) {
				RETVAL_OBJVAL(obj->body, 1);
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpMessage, setBody)
{
	zval *zbody;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &zbody, php_http_message_body_get_class_entry())) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_http_message_body_object_t *body_obj = zend_object_store_get_object(zbody TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}
		php_http_message_object_prophandler_set_body(obj, zbody TSRMLS_CC);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpMessage, addBody)
{
	zval *new_body;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &new_body, php_http_message_body_get_class_entry())) {
		php_http_message_body_object_t *old_obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_http_message_body_object_t *new_obj = zend_object_store_get_object(new_body TSRMLS_CC);

		php_http_message_body_to_callback(old_obj->body, (php_http_pass_callback_t) php_http_message_body_append, new_obj->body, 0, 0);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}


PHP_METHOD(HttpMessage, getHeader)
{
	char *header_str;
	int header_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &header_str, &header_len)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		zval *header;

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		if ((header = php_http_message_header(obj->message, header_str, header_len, 0))) {
			RETURN_ZVAL(header, 1, 1);
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpMessage, getHeaders)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		array_init(return_value);
		array_copy(&obj->message->hdrs, Z_ARRVAL_P(return_value));
	}
}

PHP_METHOD(HttpMessage, setHeader)
{
	zval *zvalue = NULL;
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z!", &name_str, &name_len, &zvalue)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *name = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

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

PHP_METHOD(HttpMessage, setHeaders)
{
	zval *new_headers = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/!", &new_headers)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		zend_hash_clean(&obj->message->hdrs);
		if (new_headers) {
			array_join(Z_ARRVAL_P(new_headers), &obj->message->hdrs, 0, ARRAY_JOIN_PRETTIFY|ARRAY_JOIN_STRONLY);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpMessage, addHeader)
{
	zval *zvalue;
	char *name_str;
	int name_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &name_str, &name_len, &zvalue)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *name = php_http_pretty_key(estrndup(name_str, name_len), name_len, 1, 1);
		zval *header;

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		Z_ADDREF_P(zvalue);
		if ((header = php_http_message_header(obj->message, name, name_len, 0))) {
			convert_to_array(header);
			zend_hash_next_index_insert(Z_ARRVAL_P(header), &zvalue, sizeof(void *), NULL);
		} else {
			zend_symtable_update(&obj->message->hdrs, name, name_len + 1, &zvalue, sizeof(void *), NULL);
		}
		efree(name);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpMessage, addHeaders)
{
	zval *new_headers;
	zend_bool append = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|b", &new_headers, &append)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		array_join(Z_ARRVAL_P(new_headers), &obj->message->hdrs, append, ARRAY_JOIN_STRONLY|ARRAY_JOIN_PRETTIFY);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpMessage, getType)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		RETURN_LONG(obj->message->type);
	}
}

PHP_METHOD(HttpMessage, setType)
{
	long type;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		php_http_message_set_type(obj->message, type);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpMessage, getInfo)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

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

PHP_METHOD(HttpMessage, setInfo)
{
	char *str;
	int len;
	php_http_info_t inf;

	if (	SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &len)
	&&		php_http_info_parse(&inf, str TSRMLS_CC)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		php_http_message_set_info(obj->message, &inf);
		php_http_info_dtor(&inf);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpMessage, getHttpVersion)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		char *str;
		size_t len;
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		php_http_version_to_string(&obj->message->http.version, &str, &len, NULL, NULL TSRMLS_CC);
		RETURN_STRINGL(str, len, 0);
	}

	RETURN_FALSE;
}

PHP_METHOD(HttpMessage, setHttpVersion)
{
	char *v_str;
	int v_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &v_str, &v_len)) {
		php_http_version_t version;
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		if (php_http_version_parse(&version, v_str TSRMLS_CC)) {
			obj->message->http.version = version;
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpMessage, getResponseCode)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		PHP_HTTP_MESSAGE_TYPE_CHECK(RESPONSE, obj->message, RETURN_FALSE);
		RETURN_LONG(obj->message->http.info.response.code);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpMessage, setResponseCode)
{
	long code;
	zend_bool strict = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|b", &code, &strict)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

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

PHP_METHOD(HttpMessage, getResponseStatus)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		PHP_HTTP_MESSAGE_TYPE_CHECK(RESPONSE, obj->message, RETURN_FALSE);
		if (obj->message->http.info.response.status) {
			RETURN_STRING(obj->message->http.info.response.status, 1);
		} else {
			RETURN_EMPTY_STRING();
		}
	}

	RETURN_FALSE;
}

PHP_METHOD(HttpMessage, setResponseStatus)
{
	char *status;
	int status_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &status, &status_len)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		PHP_HTTP_MESSAGE_TYPE_CHECK(RESPONSE, obj->message, RETURN_FALSE);
		STR_SET(obj->message->http.info.response.status, estrndup(status, status_len));
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpMessage, getRequestMethod)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		PHP_HTTP_MESSAGE_TYPE_CHECK(REQUEST, obj->message, RETURN_FALSE);
		if (obj->message->http.info.request.method) {
			RETURN_STRING(obj->message->http.info.request.method, 1);
		} else {
			RETURN_EMPTY_STRING();
		}
	}

	RETURN_FALSE;
}

PHP_METHOD(HttpMessage, setRequestMethod)
{
	char *method;
	int method_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &method, &method_len)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		PHP_HTTP_MESSAGE_TYPE_CHECK(REQUEST, obj->message, RETURN_FALSE);
		if (method_len < 1) {
			php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "Cannot set HttpMessage::requestMethod to an empty string");
			RETURN_FALSE;
		}

		STR_SET(obj->message->http.info.request.method, estrndup(method, method_len));
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpMessage, getRequestUrl)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		PHP_HTTP_MESSAGE_TYPE_CHECK(REQUEST, obj->message, RETURN_FALSE);
		if (obj->message->http.info.request.url) {
			RETURN_STRING(obj->message->http.info.request.url, 1);
		} else {
			RETURN_EMPTY_STRING();
		}
	}

	RETURN_FALSE;
}

PHP_METHOD(HttpMessage, setRequestUrl)
{
	char *url_str;
	int url_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &url_str, &url_len)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		PHP_HTTP_MESSAGE_TYPE_CHECK(REQUEST, obj->message, RETURN_FALSE);
		if (url_len < 1) {
			php_http_error(HE_WARNING, PHP_HTTP_E_INVALID_PARAM, "Cannot set HttpMessage::requestUrl to an empty string");
			RETURN_FALSE;
		}
		STR_SET(obj->message->http.info.request.url, estrndup(url_str, url_len));
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}


PHP_METHOD(HttpMessage, getParentMessage)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			if (!obj->message) {
				obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
			}

			if (obj->message->parent) {
				RETVAL_OBJVAL(obj->parent, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "HttpMessage does not have a parent message");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpMessage, toString)
{
	zend_bool include_parent = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &include_parent)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *string;
		size_t length;

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

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

PHP_METHOD(HttpMessage, toStream)
{
	zval *zstream;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zstream)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_stream *s;

		php_stream_from_zval(s, &zstream);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		php_http_message_to_callback(obj->message, (php_http_pass_callback_t) _php_stream_write, s);
	}
}

PHP_METHOD(HttpMessage, toCallback)
{
	php_http_pass_fcall_arg_t fcd;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "f", &fcd.fci, &fcd.fcc)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

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

PHP_METHOD(HttpMessage, serialize)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *string;
		size_t length;

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		php_http_message_serialize(obj->message, &string, &length);
		RETURN_STRINGL(string, length, 0);
	}
	RETURN_EMPTY_STRING();
}

PHP_METHOD(HttpMessage, unserialize)
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
		if ((msg = php_http_message_parse(NULL, serialized, (size_t) length TSRMLS_CC))) {
			obj->message = msg;
		} else {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
			php_http_error(HE_ERROR, PHP_HTTP_E_RUNTIME, "Could not unserialize HttpMessage");
		}
	}
}

PHP_METHOD(HttpMessage, detach)
{
	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			if (!obj->message) {
				obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
			}

			RETVAL_OBJVAL(php_http_message_object_new_ex(obj->zo.ce, php_http_message_copy(obj->message, NULL), NULL TSRMLS_CC), 0);
		}
	} end_error_handling();
}

PHP_METHOD(HttpMessage, prepend)
{
	zval *prepend;
	zend_bool top = 1;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|b", &prepend, php_http_message_class_entry, &top)) {
		php_http_message_t *msg[2];
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_http_message_object_t *prepend_obj = zend_object_store_get_object(prepend TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}
		if (!prepend_obj->message) {
			prepend_obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
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

PHP_METHOD(HttpMessage, reverse)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_reverse(getThis(), return_value TSRMLS_CC);
	}
}

PHP_METHOD(HttpMessage, isMultipart)
{
	zval *zboundary = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &zboundary)) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *boundary = NULL;

		RETVAL_BOOL(php_http_message_is_multipart(obj->message, zboundary ? &boundary : NULL));

		if (zboundary && boundary) {
			zval_dtor(zboundary);
			ZVAL_STRING(zboundary, boundary, 0);
		}
	}
}

PHP_METHOD(HttpMessage, splitMultipartBody)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *boundary = NULL;

		if (php_http_message_is_multipart(obj->message, &boundary)) {
			php_http_message_t *msg;

			if ((msg = php_http_message_body_split(&obj->message->body, boundary))) {
				RETVAL_OBJVAL(php_http_message_object_new_ex(php_http_message_class_entry, msg, NULL TSRMLS_CC), 0);
			}
		}
		STR_FREE(boundary);
	}
}

PHP_METHOD(HttpMessage, count)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		long i = 0;
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (!obj->message) {
			obj->message = php_http_message_init(NULL, 0 TSRMLS_CC);
		}

		php_http_message_count(i, obj->message);
		RETURN_LONG(i);
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpMessage, rewind)
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

PHP_METHOD(HttpMessage, valid)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_BOOL(obj->iterator != NULL);
	}
}

PHP_METHOD(HttpMessage, next)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->iterator) {
			php_http_message_object_t *itr = zend_object_store_get_object(obj->iterator TSRMLS_CC);

			if (itr && itr->parent.handle) {
				zval *old = obj->iterator;
				MAKE_STD_ZVAL(obj->iterator);
				ZVAL_OBJVAL(obj->iterator, itr->parent, 1);
				zval_ptr_dtor(&old);
			} else {
				zval_ptr_dtor(&obj->iterator);
				obj->iterator = NULL;
			}
		}
	}
}

PHP_METHOD(HttpMessage, key)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG(obj->iterator ? obj->iterator->value.obj.handle:0);
	}
}

PHP_METHOD(HttpMessage, current)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		if (obj->iterator) {
			RETURN_ZVAL(obj->iterator, 1, 0);
		}
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

