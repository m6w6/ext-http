/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2010, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id: http_message_body.c 292841 2009-12-31 08:48:57Z mike $ */

#include "php_http.h"

typedef struct curl_httppost *post_data[2];

static inline STATUS add_field(php_http_message_body_t *body, const char *name, const char *value_str, size_t value_len);
static inline STATUS add_file(php_http_message_body_t *body, const char *name, const char *path, const char *ctype);
static STATUS recursive_fields(post_data http_post_data, HashTable *fields, const char *prefix TSRMLS_DC);
static STATUS recursive_files(post_data http_post_data, HashTable *files, const char *prefix TSRMLS_DC);

PHP_HTTP_API php_http_message_body_t *php_http_message_body_init(php_http_message_body_t *body, php_stream *stream TSRMLS_DC)
{
	if (!body) {
		body = emalloc(sizeof(php_http_message_body_t));
	}
	
	if (stream) {
		php_stream_auto_cleanup(stream);
		body->stream_id = php_stream_get_resource_id(stream);
		zend_list_addref(body->stream_id);
	} else {
		stream = php_stream_temp_new();
		php_stream_auto_cleanup(stream);
		body->stream_id = php_stream_get_resource_id(stream);
	}
	TSRMLS_SET_CTX(body->ts);

	return body;
}

PHP_HTTP_API php_http_message_body_t *php_http_message_body_copy(php_http_message_body_t *from, php_http_message_body_t *to, zend_bool dup_internal_stream_and_contents)
{
	if (!from) {
		return NULL;
	} else {
		TSRMLS_FETCH_FROM_CTX(from->ts);
		
		if (dup_internal_stream_and_contents) {
			to = php_http_message_body_init(to, NULL TSRMLS_CC);
			php_http_message_body_to_stream(from, php_http_message_body_stream(to), 0, 0);
		} else {
			to = php_http_message_body_init(to, php_http_message_body_stream(from) TSRMLS_CC);
		}

		return to;
	}
}

PHP_HTTP_API void php_http_message_body_dtor(php_http_message_body_t *body)
{
	if (body) {
		/* NO FIXME: shows leakinfo in DEBUG mode */
		zend_list_delete(body->stream_id);
	}
}

PHP_HTTP_API void php_http_message_body_free(php_http_message_body_t **body)
{
	if (*body) {
		php_http_message_body_dtor(*body);
		efree(*body);
		*body = NULL;
	}
}

PHP_HTTP_API php_stream_statbuf *php_http_message_body_stat(php_http_message_body_t *body)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	php_stream_stat(php_http_message_body_stream(body), &body->ssb);
	return &body->ssb;
}

PHP_HTTP_API char *php_http_message_body_etag(php_http_message_body_t *body)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	php_stream_statbuf *ssb = php_http_message_body_stat(body);

	/* real file or temp buffer ? */
	if (body->ssb.sb.st_mtime) {
		char *etag;

		spprintf(&etag, 0, "%lx-%lx-%lx", ssb->sb.st_ino, ssb->sb.st_mtime, ssb->sb.st_size);
		return etag;
	} else {
		void *ctx = php_http_etag_init(TSRMLS_C);

		php_http_message_body_to_callback(body, php_http_etag_update, ctx, 0, 0);
		return php_http_etag_finish(ctx TSRMLS_CC);
	}
}

PHP_HTTP_API void php_http_message_body_to_string(php_http_message_body_t *body, char **buf, size_t *len, off_t offset, size_t forlen)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	php_stream *s = php_http_message_body_stream(body);

	php_stream_seek(s, offset, SEEK_SET);
	if (!forlen) {
		forlen = -1;
	}
	*len = php_stream_copy_to_mem(s, buf, forlen, 0);
}

PHP_HTTP_API void php_http_message_body_to_stream(php_http_message_body_t *body, php_stream *dst, off_t offset, size_t forlen)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	php_stream *s = php_http_message_body_stream(body);

	php_stream_seek(s, offset, SEEK_SET);
	if (!forlen) {
		forlen = -1;
	}
	php_stream_copy_to_stream_ex(s, dst, forlen, NULL);
}

PHP_HTTP_API void php_http_message_body_to_callback(php_http_message_body_t *body, php_http_pass_callback_t cb, void *cb_arg, off_t offset, size_t forlen)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	php_stream *s = php_http_message_body_stream(body);

	php_stream_seek(s, offset, SEEK_SET);

	if (!forlen) {
		forlen = -1;
	}
	while (!php_stream_eof(s)) {
		char buf[0x1000];
		size_t read = php_stream_read(s, buf, MIN(forlen, sizeof(buf)));

		if (read) {
			cb(cb_arg, buf, read);
		}

		if (read < MIN(forlen, sizeof(buf))) {
			break;
		}

		if (forlen && !(forlen -= read)) {
			break;
		}
	}
}

PHP_HTTP_API STATUS php_http_message_body_to_callback_in_chunks(php_http_message_body_t *body, php_http_pass_callback_t cb, void *cb_arg, HashTable *chunks)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	php_stream *s = php_http_message_body_stream(body);
	HashPosition pos;
	zval **chunk;

	FOREACH_HASH_VAL(pos, chunks, chunk) {
		zval **begin, **end;

		if (IS_ARRAY == Z_TYPE_PP(chunk)
		&&	SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(chunk), 0, (void *) &begin)
		&&	IS_LONG == Z_TYPE_PP(begin)
		&&	SUCCESS == zend_hash_index_find(Z_ARRVAL_PP(chunk), 1, (void *) &end)
		&&	IS_LONG == Z_TYPE_PP(end)
		) {
			if (SUCCESS != php_stream_seek(s, Z_LVAL_PP(begin), SEEK_SET)) {
				return FAILURE;
			} else {
				long length = Z_LVAL_PP(end) - Z_LVAL_PP(begin) + 1;

				while (length > 0 && !php_stream_eof(s)) {
					char buf[0x1000];
					size_t read = php_stream_read(s, buf, MIN(length, sizeof(buf)));

					if (read) {
						cb(cb_arg, buf, read);
					}

					if (read < MIN(length, sizeof(buf))) {
						break;
					}

					length -= read;
				}
			}
		}
	}

	return SUCCESS;
}

PHP_HTTP_API size_t php_http_message_body_append(php_http_message_body_t *body, const char *buf, size_t len)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	php_stream *s = php_http_message_body_stream(body);

	php_stream_seek(s, 0, SEEK_END);
	return php_stream_write(s, buf, len);
}

PHP_HTTP_API STATUS php_http_message_body_add(php_http_message_body_t *body, HashTable *fields, HashTable *files)
{
	post_data http_post_data = {NULL, NULL};
	TSRMLS_FETCH_FROM_CTX(body->ts);

	if (fields && SUCCESS != recursive_fields(http_post_data, fields, NULL TSRMLS_CC)) {
		return FAILURE;
	}
	if (files && (zend_hash_num_elements(files) > 0) && (SUCCESS != recursive_files(http_post_data, files, NULL TSRMLS_CC))) {
		return FAILURE;
	}
	if (CURLE_OK != curl_formget(http_post_data[0], body, (curl_formget_callback) php_http_message_body_append)) {
		return FAILURE;
	}
	return SUCCESS;
}

PHP_HTTP_API STATUS php_http_message_body_add_field(php_http_message_body_t *body, const char *name, const char *value_str, size_t value_len)
{
	return add_field(body, name, value_str, value_len);
}

PHP_HTTP_API STATUS php_http_message_body_add_file(php_http_message_body_t *body, const char *name, const char *path, const char *ctype)
{
	return add_file(body, name, path, ctype);
}

static inline char *format_key(uint type, char *str, ulong num, const char *prefix, int numeric_key_for_empty_prefix) {
	char *new_key = NULL;
	
	if (prefix && *prefix) {
		if (type == HASH_KEY_IS_STRING) {
			spprintf(&new_key, 0, "%s[%s]", prefix, str);
		} else {
			spprintf(&new_key, 0, "%s[%lu]", prefix, num);
		}
	} else if (type == HASH_KEY_IS_STRING) {
		new_key = estrdup(str);
	} else if (numeric_key_for_empty_prefix) {
		spprintf(&new_key, 0, "%lu", num);
	}
	
	return new_key;
}

static inline STATUS add_field(php_http_message_body_t *body, const char *name, const char *value_str, size_t value_len)
{
	post_data http_post_data = {NULL, NULL};
	CURLcode err;

	err = curl_formadd(&http_post_data[0], &http_post_data[1],
		CURLFORM_COPYNAME, name,
		CURLFORM_COPYCONTENTS, value_str,
		CURLFORM_CONTENTSLENGTH, (long) value_len,
		CURLFORM_END
	);

	if (CURLE_OK != err) {
		return FAILURE;
	}

	err = curl_formget(http_post_data[0], body, (curl_formget_callback) php_http_message_body_append);

	if (CURLE_OK != err) {
		curl_formfree(http_post_data[0]);
		return FAILURE;
	}

	curl_formfree(http_post_data[0]);
	return SUCCESS;
}

static inline STATUS add_file(php_http_message_body_t *body, const char *name, const char *path, const char *ctype)
{
	post_data http_post_data = {NULL, NULL};
	CURLcode err;

	err = curl_formadd(&http_post_data[0], &http_post_data[1],
		CURLFORM_COPYNAME, name,
		CURLFORM_FILE, path,
		CURLFORM_CONTENTTYPE, ctype ? ctype : "application/octet-stream",
		CURLFORM_END
	);

	if (CURLE_OK != err) {
		return FAILURE;
	}

	err = curl_formget(http_post_data[0], body, (curl_formget_callback) php_http_message_body_append);

	if (CURLE_OK != err) {
		curl_formfree(http_post_data[0]);
		return FAILURE;
	}

	curl_formfree(http_post_data[0]);
	return SUCCESS;
}

static STATUS recursive_fields(post_data http_post_data, HashTable *fields, const char *prefix TSRMLS_DC) {
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	zval **data_ptr;
	HashPosition pos;
	char *new_key = NULL;
	CURLcode err = 0;
	
	if (fields && !fields->nApplyCount) {
		FOREACH_HASH_KEYVAL(pos, fields, key, data_ptr) {
			if (key.type != HASH_KEY_IS_STRING || *key.str) {
				new_key = format_key(key.type, key.str, key.num, prefix, 1);
				
				switch (Z_TYPE_PP(data_ptr)) {
					case IS_ARRAY:
					case IS_OBJECT: {
						STATUS status;
						
						++fields->nApplyCount;
						status = recursive_fields(http_post_data, HASH_OF(*data_ptr), new_key TSRMLS_CC);
						--fields->nApplyCount;
						
						if (SUCCESS != status) {
							goto error;
						}
						break;
					}
						
					default: {
						zval *data = php_http_zsep(IS_STRING, *data_ptr);
						
						err = curl_formadd(&http_post_data[0], &http_post_data[1],
							CURLFORM_COPYNAME,			new_key,
							CURLFORM_COPYCONTENTS,		Z_STRVAL_P(data),
							CURLFORM_CONTENTSLENGTH,	(long) Z_STRLEN_P(data),
							CURLFORM_END
						);
						
						zval_ptr_dtor(&data);
						
						if (CURLE_OK != err) {
							goto error;
						}
						break;
					}
				}
				STR_FREE(new_key);
			}
		}
	}
	
	return SUCCESS;
	
error:
	if (new_key) {
		efree(new_key);
	}
	if (http_post_data[0]) {
		curl_formfree(http_post_data[0]);
	}
	if (err) {
		php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Could not encode post fields: %s", curl_easy_strerror(err));
	} else {
		php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Could not encode post fields: unknown error");
	}
	return FAILURE;
}

static STATUS recursive_files(post_data http_post_data, HashTable *files, const char *prefix TSRMLS_DC) {
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	zval **data_ptr;
	HashPosition pos;
	char *new_key = NULL;
	CURLcode err = 0;
	
	if (files && !files->nApplyCount) {
		FOREACH_HASH_KEYVAL(pos, files, key, data_ptr) {
			zval **file_ptr, **type_ptr, **name_ptr;
			
			if (key.type != HASH_KEY_IS_STRING || *key.str) {
				new_key = format_key(key.type, key.str, key.num, prefix, 0);
				
				if (Z_TYPE_PP(data_ptr) != IS_ARRAY && Z_TYPE_PP(data_ptr) != IS_OBJECT) {
					if (new_key || key.type == HASH_KEY_IS_STRING) {
						php_http_error(HE_NOTICE, PHP_HTTP_E_INVALID_PARAM, "Unrecognized type of post file array entry '%s'", new_key ? new_key : key.str);
					} else {
						php_http_error(HE_NOTICE, PHP_HTTP_E_INVALID_PARAM, "Unrecognized type of post file array entry '%lu'", key.num);
					}
				} else if (	SUCCESS != zend_hash_find(HASH_OF(*data_ptr), "name", sizeof("name"), (void *) &name_ptr) ||
							SUCCESS != zend_hash_find(HASH_OF(*data_ptr), "type", sizeof("type"), (void *) &type_ptr) ||
							SUCCESS != zend_hash_find(HASH_OF(*data_ptr), "file", sizeof("file"), (void *) &file_ptr)) {
					STATUS status;
					
					++files->nApplyCount;
					status = recursive_files(http_post_data, HASH_OF(*data_ptr), new_key TSRMLS_CC);
					--files->nApplyCount;
					
					if (SUCCESS != status) {
						goto error;
					}
				} else {
					const char *path;
					zval *file = php_http_zsep(IS_STRING, *file_ptr);
					zval *type = php_http_zsep(IS_STRING, *type_ptr);
					zval *name = php_http_zsep(IS_STRING, *name_ptr);
					
					if (SUCCESS != php_check_open_basedir(Z_STRVAL_P(file) TSRMLS_CC)) {
						goto error;
					}
					
					/* this is blatant but should be sufficient for most cases */
					if (strncasecmp(Z_STRVAL_P(file), "file://", lenof("file://"))) {
						path = Z_STRVAL_P(file);
					} else {
						path = Z_STRVAL_P(file) + lenof("file://");
					}
					
					if (new_key) {
						char *tmp_key = format_key(HASH_KEY_IS_STRING, Z_STRVAL_P(name), 0, new_key, 0);
						STR_SET(new_key, tmp_key);
					}
					
					err = curl_formadd(&http_post_data[0], &http_post_data[1],
						CURLFORM_COPYNAME,		new_key ? new_key : Z_STRVAL_P(name),
						CURLFORM_FILE,			path,
						CURLFORM_CONTENTTYPE,	Z_STRVAL_P(type),
						CURLFORM_END
					);
					
					zval_ptr_dtor(&file);
					zval_ptr_dtor(&type);
					zval_ptr_dtor(&name);
					
					if (CURLE_OK != err) {
						goto error;
					}
				}
				STR_FREE(new_key);
			}
		}
	}
	
	return SUCCESS;
	
error:
	if (new_key) {
		efree(new_key);
	}
	if (http_post_data[0]) {
		curl_formfree(http_post_data[0]);
	}
	if (err) {
		php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Could not encode post files: %s", curl_easy_strerror(err));
	} else {
		php_http_error(HE_WARNING, PHP_HTTP_E_ENCODING, "Could not encode post files: unknown error");
	}
	return FAILURE;
}

/* PHP */

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpMessageBody, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpMessageBody, method, 0)
#define PHP_HTTP_MESSAGE_BODY_ME(method, visibility)	PHP_ME(HttpMessageBody, method, PHP_HTTP_ARGS(HttpMessageBody, method), visibility)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(stream, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(__toString);

PHP_HTTP_BEGIN_ARGS(toStream, 1)
	PHP_HTTP_ARG_VAL(stream, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(toCallback, 1)
	PHP_HTTP_ARG_VAL(callback, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(append, 1)
	PHP_HTTP_ARG_VAL(string, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(add, 0)
	PHP_HTTP_ARG_VAL(fields, 0)
	PHP_HTTP_ARG_VAL(files, 0)
PHP_HTTP_END_ARGS;


zend_class_entry *php_http_message_body_class_entry;
zend_function_entry php_http_message_body_method_entry[] = {
	PHP_HTTP_MESSAGE_BODY_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_MESSAGE_BODY_ME(__toString, ZEND_ACC_PUBLIC)
	PHP_MALIAS(HttpMessageBody, toString, __toString, args_for_HttpMessageBody___toString, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(toStream, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(toCallback, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(append, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(add, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers php_http_message_body_object_handlers;

PHP_MINIT_FUNCTION(http_message_body)
{
	PHP_HTTP_REGISTER_CLASS(http\\message, Body, http_message_body, php_http_object_class_entry, 0);
	php_http_message_body_class_entry->create_object = php_http_message_body_object_new;
	memcpy(&php_http_message_body_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_message_body_object_handlers.clone_obj = php_http_message_body_object_clone;

	return SUCCESS;
}

zend_object_value php_http_message_body_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_message_body_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_message_body_object_new_ex(zend_class_entry *ce, php_http_message_body_t *body, php_http_message_body_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_message_body_object_t *o;

	o = ecalloc(1, sizeof(php_http_message_body_object_t));
	zend_object_std_init((zend_object *) o, php_http_message_body_class_entry TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (ptr) {
		*ptr = o;
	}

	if (body) {
		o->body = body;
	}

	ov.handle = zend_objects_store_put((zend_object *) o, NULL, php_http_message_body_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_message_body_object_handlers;

	return ov;
}

zend_object_value php_http_message_body_object_clone(zval *object TSRMLS_DC)
{
	zend_object_value new_ov;
	php_http_message_body_object_t *new_obj = NULL;
	php_http_message_body_object_t *old_obj = zend_object_store_get_object(object TSRMLS_CC);

	new_ov = php_http_message_body_object_new_ex(old_obj->zo.ce, php_http_message_body_copy(old_obj->body, NULL, 1), &new_obj);
	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(object) TSRMLS_CC);

	return new_ov;
}

void php_http_message_body_object_free(void *object TSRMLS_DC)
{
	php_http_message_body_object_t *obj = object;

	php_http_message_body_free(&obj->body);

	zend_object_std_dtor((zend_object *) obj TSRMLS_CC);
	efree(obj);
}

PHP_METHOD(HttpMessageBody, __construct)
{
	php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
	zval *zstream;
	php_stream *stream;

	with_error_handling(EH_THROW, PHP_HTTP_EX_CE(runtime)) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zstream)) {
			php_stream_from_zval(stream, &zstream);

			if (stream) {
				if (obj->body) {
					php_http_message_body_dtor(obj->body);
				}
				obj->body = php_http_message_body_init(obj->body, stream TSRMLS_CC);
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpMessageBody, __toString)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *str;
		size_t len;

		php_http_message_body_to_string(obj->body, &str, &len, 0, 0);
		if (str) {
			RETURN_STRINGL(str, len, 0);
		}
	}
	RETURN_EMPTY_STRING();
}

PHP_METHOD(HttpMessageBody, toStream)
{
	zval *zstream;
	long offset = 0, forlen = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|ll", &zstream, &offset, &forlen)) {
		php_stream *stream;
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_stream_from_zval(stream, &zstream);
		php_http_message_body_to_stream(obj->body, stream, offset, forlen);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

struct fcd {
	zval *fcz;
	zend_fcall_info *fci;
	zend_fcall_info_cache *fcc;
};

static size_t pass(void *cb_arg, const char *str, size_t len TSRMLS_DC)
{
	struct fcd *fcd = cb_arg;
	zval *zdata;

	MAKE_STD_ZVAL(zdata);
	ZVAL_STRINGL(zdata, str, len, 1);
	if (SUCCESS == zend_fcall_info_argn(fcd->fci TSRMLS_CC, 2, fcd->fcz, zdata)) {
		zend_fcall_info_call(fcd->fci, fcd->fcc, NULL, NULL TSRMLS_CC);
		zend_fcall_info_args_clear(fcd->fci, 0);
	}
	zval_ptr_dtor(&zdata);
	return len;
}

PHP_METHOD(HttpMessageBody, toCallback)
{
	struct fcd fcd;
	long offset = 0, forlen = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "f|ll", &fcd.fci, &fcd.fcc, &offset, &forlen)) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		fcd.fcz = getThis();
		Z_ADDREF_P(fcd.fcz);
		php_http_message_body_to_callback(obj->body, pass, &fcd, offset, forlen);
		zval_ptr_dtor(&fcd.fcz);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpMessageBody, append)
{
	char *str;
	int len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &len)) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_LONG(php_http_message_body_append(obj->body, str, len));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpMessageBody, add)
{
	HashTable *fields = NULL, *files = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|hh", &fields, &files)) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_SUCCESS(php_http_message_body_add(obj->body, fields, files));
	}
	RETURN_FALSE;
}
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
