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

#include <libgen.h>
#include <ext/standard/php_lcg.h>
#include <ext/standard/php_string.h>

#define BOUNDARY_OPEN(body) \
	do {\
		size_t size = php_http_message_body_size(body); \
		if (size) { \
			php_stream_truncate_set_size(php_http_message_body_stream(body), size - lenof("--" PHP_HTTP_CRLF)); \
			php_http_message_body_append(body, ZEND_STRL(PHP_HTTP_CRLF)); \
		} else { \
			php_http_message_body_appendf(body, "--%s" PHP_HTTP_CRLF, php_http_message_body_boundary(body)); \
		} \
	} while(0)
#define BOUNDARY_CLOSE(body) \
		php_http_message_body_appendf(body, PHP_HTTP_CRLF "--%s--" PHP_HTTP_CRLF, php_http_message_body_boundary(body))

static STATUS add_recursive_fields(php_http_message_body_t *body, const char *name, zval *value);
static STATUS add_recursive_files(php_http_message_body_t *body, const char *name, zval *value);

PHP_HTTP_API php_http_message_body_t *php_http_message_body_init(php_http_message_body_t *body, php_stream *stream TSRMLS_DC)
{
	if (!body) {
		body = emalloc(sizeof(php_http_message_body_t));
	}
	memset(body, 0, sizeof(*body));
	
	if (stream) {
		php_stream_auto_cleanup(stream);
		body->stream_id = php_stream_get_resource_id(stream);
		zend_list_addref(body->stream_id);
	} else {
		stream = php_stream_temp_create(TEMP_STREAM_DEFAULT, 0xffff);
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

		if (from->boundary) {
			to->boundary = estrdup(from->boundary);
		}

		return to;
	}
}

PHP_HTTP_API void php_http_message_body_dtor(php_http_message_body_t *body)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	/* NO FIXME: shows leakinfo in DEBUG mode */
	zend_list_delete(body->stream_id);
	STR_FREE(body->boundary);
}

PHP_HTTP_API void php_http_message_body_free(php_http_message_body_t **body)
{
	if (*body) {
		php_http_message_body_dtor(*body);
		efree(*body);
		*body = NULL;
	}
}

PHP_HTTP_API const php_stream_statbuf *php_http_message_body_stat(php_http_message_body_t *body)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	php_stream_stat(php_http_message_body_stream(body), &body->ssb);
	return &body->ssb;
}

PHP_HTTP_API const char *php_http_message_body_boundary(php_http_message_body_t *body)
{
	if (!body->boundary) {
		union { double dbl; int num[2]; } data;
		TSRMLS_FETCH_FROM_CTX(body->ts);

		data.dbl = php_combined_lcg(TSRMLS_C);
		spprintf(&body->boundary, 0, "%x.%x", data.num[0], data.num[1]);
	}
	return body->boundary;
}

PHP_HTTP_API char *php_http_message_body_etag(php_http_message_body_t *body)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);
	const php_stream_statbuf *ssb = php_http_message_body_stat(body);

	/* real file or temp buffer ? */
	if (body->ssb.sb.st_mtime) {
		char *etag;

		spprintf(&etag, 0, "%lx-%lx-%lx", ssb->sb.st_ino, ssb->sb.st_mtime, ssb->sb.st_size);
		return etag;
	} else {
		php_http_etag_t *etag = php_http_etag_init(PHP_HTTP_G->env.etag_mode TSRMLS_CC);

		php_http_message_body_to_callback(body, (php_http_pass_callback_t) php_http_etag_update, etag, 0, 0);
		return php_http_etag_finish(etag);
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
	char *buf = emalloc(0x1000);

	php_stream_seek(s, offset, SEEK_SET);

	if (!forlen) {
		forlen = -1;
	}
	while (!php_stream_eof(s)) {
		size_t read = php_stream_read(s, buf, MIN(forlen, 0x1000));

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
	efree(buf);
}

PHP_HTTP_API size_t php_http_message_body_append(php_http_message_body_t *body, const char *buf, size_t len)
{
	php_stream *s;
	TSRMLS_FETCH_FROM_CTX(body->ts);

	s = php_http_message_body_stream(body);
	php_stream_seek(s, 0, SEEK_END);
	return php_stream_write(s, buf, len);
}

PHP_HTTP_API size_t php_http_message_body_appendf(php_http_message_body_t *body, const char *fmt, ...)
{
	va_list argv;
	char *print_str;
	size_t print_len;

	va_start(argv, fmt);
	print_len = vspprintf(&print_str, 0, fmt, argv);
	va_end(argv);

	print_len = php_http_message_body_append(body, print_str, print_len);
	efree(print_str);

	return print_len;
}

PHP_HTTP_API STATUS php_http_message_body_add(php_http_message_body_t *body, HashTable *fields, HashTable *files)
{
	zval tmp;

	if (fields) {
		INIT_PZVAL_ARRAY(&tmp, fields);
		if (SUCCESS != add_recursive_fields(body, NULL, &tmp)) {
			return FAILURE;
		}
	}
	if (files) {
		INIT_PZVAL_ARRAY(&tmp, files);
		if (SUCCESS != add_recursive_files(body, NULL, &tmp)) {
			return FAILURE;
		}
	}

	return SUCCESS;
}


PHP_HTTP_API STATUS php_http_message_body_add_field(php_http_message_body_t *body, const char *name, const char *value_str, size_t value_len)
{
	char *safe_name;
	TSRMLS_FETCH_FROM_CTX(body->ts);

	safe_name = php_addslashes(estrdup(name), strlen(name), NULL, 1 TSRMLS_CC);

	BOUNDARY_OPEN(body);
	php_http_message_body_appendf(
		body,
			"Content-Disposition: form-data; name=\"%s\"" PHP_HTTP_CRLF
			"" PHP_HTTP_CRLF,
		safe_name);
	php_http_message_body_append(body, value_str, value_len);
	BOUNDARY_CLOSE(body);

	efree(safe_name);
	return SUCCESS;
}

PHP_HTTP_API STATUS php_http_message_body_add_file(php_http_message_body_t *body, const char *name, const char *path, const char *ctype)
{
	php_stream *in;
	char *path_dup = estrdup(path);
	TSRMLS_FETCH_FROM_CTX(body->ts);

	if ((in = php_stream_open_wrapper(path_dup, "r", REPORT_ERRORS|USE_PATH|STREAM_MUST_SEEK, NULL))) {
		php_stream_statbuf ssb = {{0}};

		if (SUCCESS == php_stream_stat(in, &ssb) && S_ISREG(ssb.sb.st_mode)) {
			char *safe_name = php_addslashes(estrdup(name), strlen(name), NULL, 1 TSRMLS_CC);

			BOUNDARY_OPEN(body);
			php_http_message_body_appendf(
				body,
				"Content-Disposition: attachment; name=\"%s\"; filename=\"%s\""	PHP_HTTP_CRLF
				"Content-Type: %s" PHP_HTTP_CRLF
				"Content-Length: %zu" PHP_HTTP_CRLF
				"" PHP_HTTP_CRLF,
				safe_name, basename(path_dup), ctype, ssb.sb.st_size);
			php_stream_copy_to_stream_ex(in, php_http_message_body_stream(body), PHP_STREAM_COPY_ALL, NULL);
			BOUNDARY_CLOSE(body);

			efree(safe_name);
			efree(path_dup);
			php_stream_close(in);
			return SUCCESS;
		} else {
			efree(path_dup);
			php_stream_close(in);
			php_http_error(HE_WARNING, PHP_HTTP_E_MESSAGE_BODY, "Not a valid regular file: %s", path);
			return FAILURE;
		}
	} else {
		efree(path_dup);
		return FAILURE;
	}

}

static inline char *format_key(uint type, char *str, ulong num, const char *prefix) {
	char *new_key = NULL;

	if (prefix && *prefix) {
		if (type == HASH_KEY_IS_STRING) {
			spprintf(&new_key, 0, "%s[%s]", prefix, str);
		} else {
			spprintf(&new_key, 0, "%s[%lu]", prefix, num);
		}
	} else if (type == HASH_KEY_IS_STRING) {
		new_key = estrdup(str);
	} else {
		spprintf(&new_key, 0, "%lu", num);
	}

	return new_key;
}

static STATUS add_recursive_fields(php_http_message_body_t *body, const char *name, zval *value)
{
	if (Z_TYPE_P(value) == IS_ARRAY || Z_TYPE_P(value) == IS_OBJECT) {
		zval **val;
		HashPosition pos;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
		TSRMLS_FETCH_FROM_CTX(body->ts);

		if (!HASH_OF(value)->nApplyCount) {
			++HASH_OF(value)->nApplyCount;
			FOREACH_KEYVAL(pos, value, key, val) {
				char *str = format_key(key.type, key.str, key.num, name);
				if (SUCCESS != add_recursive_fields(body, str, *val)) {
					efree(str);
					HASH_OF(value)->nApplyCount--;
					return FAILURE;
				}
				efree(str);
			}
			--HASH_OF(value)->nApplyCount;
		}
	} else {
		zval *cpy = php_http_ztyp(IS_STRING, value);
		php_http_message_body_add_field(body, name, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		zval_ptr_dtor(&cpy);
	}

	return SUCCESS;
}

static STATUS add_recursive_files(php_http_message_body_t *body, const char *name, zval *value)
{
	if (Z_TYPE_P(value) == IS_ARRAY || Z_TYPE_P(value) == IS_OBJECT) {
		zval **zfile, **zname, **ztype;
		TSRMLS_FETCH_FROM_CTX(body->ts);

		if ((SUCCESS == zend_hash_find(HASH_OF(value), ZEND_STRS("name"), (void *) &zname))
		&&	(SUCCESS == zend_hash_find(HASH_OF(value), ZEND_STRS("file"), (void *) &zfile))
		&&	(SUCCESS == zend_hash_find(HASH_OF(value), ZEND_STRS("type"), (void *) &ztype))
		) {
			zval *zfc = php_http_ztyp(IS_STRING, *zfile), *znc = php_http_ztyp(IS_STRING, *zname), *ztc = php_http_ztyp(IS_STRING, *ztype);
			char *str = format_key(HASH_KEY_IS_STRING, Z_STRVAL_P(znc), 0, name);
			STATUS ret = php_http_message_body_add_file(body, str, Z_STRVAL_P(zfc), Z_STRVAL_P(ztc));

			efree(str);
			zval_ptr_dtor(&znc);
			zval_ptr_dtor(&zfc);
			zval_ptr_dtor(&ztc);

			if (ret != SUCCESS) {
				return ret;
			}
		} else {
			zval **val;
			HashPosition pos;
			php_http_array_hashkey_t key = php_http_array_hashkey_init(0);

			if (!HASH_OF(value)->nApplyCount) {
				++HASH_OF(value)->nApplyCount;
				FOREACH_KEYVAL(pos, value, key, val) {
					char *str = format_key(key.type, key.str, key.num, name);
					if (SUCCESS != add_recursive_files(body, str, *val)) {
						efree(str);
						--HASH_OF(value)->nApplyCount;
						return FAILURE;
					}
					efree(str);
				}
				--HASH_OF(value)->nApplyCount;
			}
		}
	} else {
		TSRMLS_FETCH_FROM_CTX(body->ts);
		php_http_error(HE_WARNING, PHP_HTTP_E_MESSAGE_BODY, "Unrecognized array format for message body file to add");
		return FAILURE;
	}

	return SUCCESS;
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
	PHP_HTTP_REGISTER_CLASS(http\\Message, Body, http_message_body, php_http_object_class_entry, 0);
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

	new_ov = php_http_message_body_object_new_ex(old_obj->zo.ce, php_http_message_body_copy(old_obj->body, NULL, 1), &new_obj TSRMLS_CC);
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
	zval *zstream = NULL;
	php_stream *stream;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|r!", &zstream)) {
			if (zstream) {
				php_stream_from_zval(stream, &zstream);

				if (stream) {
					if (obj->body) {
						php_http_message_body_dtor(obj->body);
					}
					obj->body = php_http_message_body_init(obj->body, stream TSRMLS_CC);
				}
			}
			if (!obj->body) {
				obj->body = php_http_message_body_init(NULL, NULL TSRMLS_CC);
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
#ifdef ZTS
	void ***ts;
#endif
};

static size_t pass(void *cb_arg, const char *str, size_t len)
{
	struct fcd *fcd = cb_arg;
	zval *zdata;
	TSRMLS_FETCH_FROM_CTX(fcd->ts);

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
