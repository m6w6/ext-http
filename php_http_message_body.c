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

#include <ext/standard/php_lcg.h>

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
	const php_stream_statbuf *ssb = php_http_message_body_stat(body);
	TSRMLS_FETCH_FROM_CTX(body->ts);

	/* real file or temp buffer ? */
	if (ssb && ssb->sb.st_mtime) {
		char *etag;

		spprintf(&etag, 0, "%lx-%lx-%lx", ssb->sb.st_ino, ssb->sb.st_mtime, ssb->sb.st_size);
		return etag;
	} else {
		php_http_etag_t *etag = php_http_etag_init(PHP_HTTP_G->env.etag_mode TSRMLS_CC);

		if (etag) {
			php_http_message_body_to_callback(body, (php_http_pass_callback_t) php_http_etag_update, etag, 0, 0);
			return php_http_etag_finish(etag);
		} else {
			return NULL;
		}
	}
}

PHP_HTTP_API void php_http_message_body_to_string(php_http_message_body_t *body, char **buf, size_t *len, off_t offset, size_t forlen)
{
	php_stream *s = php_http_message_body_stream(body);
	TSRMLS_FETCH_FROM_CTX(body->ts);

	php_stream_seek(s, offset, SEEK_SET);
	if (!forlen) {
		forlen = -1;
	}
	*len = php_stream_copy_to_mem(s, buf, forlen, 0);
}

PHP_HTTP_API void php_http_message_body_to_stream(php_http_message_body_t *body, php_stream *dst, off_t offset, size_t forlen)
{
	php_stream *s = php_http_message_body_stream(body);
	TSRMLS_FETCH_FROM_CTX(body->ts);

	php_stream_seek(s, offset, SEEK_SET);
	if (!forlen) {
		forlen = -1;
	}
	php_stream_copy_to_stream_ex(s, dst, forlen, NULL);
}

PHP_HTTP_API void php_http_message_body_to_callback(php_http_message_body_t *body, php_http_pass_callback_t cb, void *cb_arg, off_t offset, size_t forlen)
{
	php_stream *s = php_http_message_body_stream(body);
	char *buf = emalloc(0x1000);
	TSRMLS_FETCH_FROM_CTX(body->ts);

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

PHP_HTTP_API STATUS php_http_message_body_add_form(php_http_message_body_t *body, HashTable *fields, HashTable *files)
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

PHP_HTTP_API void php_http_message_body_add_part(php_http_message_body_t *body, php_http_message_t *part)
{
	TSRMLS_FETCH_FROM_CTX(body->ts);

	BOUNDARY_OPEN(body);
	php_http_message_to_callback(part, (php_http_pass_callback_t) php_http_message_body_append, body);
	BOUNDARY_CLOSE(body);
}


PHP_HTTP_API STATUS php_http_message_body_add_form_field(php_http_message_body_t *body, const char *name, const char *value_str, size_t value_len)
{
	char *safe_name;
	TSRMLS_FETCH_FROM_CTX(body->ts);

	safe_name = php_addslashes(estrdup(name), strlen(name), NULL, 1 TSRMLS_CC);

	BOUNDARY_OPEN(body);
	php_http_message_body_appendf(
		body,
		"Content-Disposition: form-data; name=\"%s\"" PHP_HTTP_CRLF
		"" PHP_HTTP_CRLF,
		safe_name
	);
	php_http_message_body_append(body, value_str, value_len);
	BOUNDARY_CLOSE(body);

	efree(safe_name);
	return SUCCESS;
}

PHP_HTTP_API STATUS php_http_message_body_add_form_file(php_http_message_body_t *body, const char *name, const char *ctype, const char *path, php_stream *in)
{
	char *safe_name, *path_dup = estrdup(path), *bname;
	size_t bname_len;
	TSRMLS_FETCH_FROM_CTX(body->ts);

	safe_name = php_addslashes(estrdup(name), strlen(name), NULL, 1 TSRMLS_CC);
	
	php_basename(path_dup, strlen(path_dup), NULL, 0, &bname, &bname_len TSRMLS_CC); 

	BOUNDARY_OPEN(body);
	php_http_message_body_appendf(
		body,
		"Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"" PHP_HTTP_CRLF
		"Content-Transfer-Encoding: binary" PHP_HTTP_CRLF
		"Content-Type: %s" PHP_HTTP_CRLF
		PHP_HTTP_CRLF,
		safe_name, bname, ctype
	);
	php_stream_copy_to_stream_ex(in, php_http_message_body_stream(body), PHP_STREAM_COPY_ALL, NULL);
	BOUNDARY_CLOSE(body);

	efree(safe_name);
	efree(path_dup);

	return SUCCESS;
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
		new_key = estrdup("");
	}

	return new_key;
}

static STATUS add_recursive_fields(php_http_message_body_t *body, const char *name, zval *value)
{
	if (Z_TYPE_P(value) == IS_ARRAY || Z_TYPE_P(value) == IS_OBJECT) {
		zval **val;
		HashTable *ht;
		HashPosition pos;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
		TSRMLS_FETCH_FROM_CTX(body->ts);

		ht = HASH_OF(value);
		if (!ht->nApplyCount) {
			++ht->nApplyCount;
			FOREACH_KEYVAL(pos, value, key, val) {
				char *str = format_key(key.type, key.str, key.num, name);
				if (SUCCESS != add_recursive_fields(body, str, *val)) {
					efree(str);
					ht->nApplyCount--;
					return FAILURE;
				}
				efree(str);
			}
			--ht->nApplyCount;
		}
	} else {
		zval *cpy = php_http_ztyp(IS_STRING, value);
		php_http_message_body_add_form_field(body, name, Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		zval_ptr_dtor(&cpy);
	}

	return SUCCESS;
}

static STATUS add_recursive_files(php_http_message_body_t *body, const char *name, zval *value)
{
	zval **zdata = NULL, **zfile, **zname, **ztype;
	HashTable *ht;
	TSRMLS_FETCH_FROM_CTX(body->ts);

	if (Z_TYPE_P(value) != IS_ARRAY && Z_TYPE_P(value) != IS_OBJECT) {
		php_http_error(HE_WARNING, PHP_HTTP_E_MESSAGE_BODY, "Expected array or object (name, type, file) for message body file to add");
		return FAILURE;
	}

	ht = HASH_OF(value);

	if ((SUCCESS != zend_hash_find(ht, ZEND_STRS("name"), (void *) &zname))
	||	(SUCCESS != zend_hash_find(ht, ZEND_STRS("type"), (void *) &ztype))
	||	(SUCCESS != zend_hash_find(ht, ZEND_STRS("file"), (void *) &zfile))
	) {
		zval **val;
		HashPosition pos;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);

		if (!ht->nApplyCount) {
			++ht->nApplyCount;
			FOREACH_HASH_KEYVAL(pos, ht, key, val) {
				if (Z_TYPE_PP(val) == IS_ARRAY || Z_TYPE_PP(val) == IS_OBJECT) {
					char *str = format_key(key.type, key.str, key.num, name);

					if (SUCCESS != add_recursive_files(body, str, *val)) {
						efree(str);
						--ht->nApplyCount;
						return FAILURE;
					}
					efree(str);
				}
			}
			--ht->nApplyCount;
		}
		return SUCCESS;
	} else {
		php_stream *stream;
		zval *zfc = php_http_ztyp(IS_STRING, *zfile);

		if (SUCCESS == zend_hash_find(ht, ZEND_STRS("data"), (void *) &zdata)) {
			if (Z_TYPE_PP(zdata) == IS_RESOURCE) {
				php_stream_from_zval_no_verify(stream, zdata);
			} else {
				zval *tmp = php_http_ztyp(IS_STRING, *zdata);

				stream = php_stream_memory_open(TEMP_STREAM_READONLY, Z_STRVAL_P(tmp), Z_STRLEN_P(tmp));
				zval_ptr_dtor(&tmp);
			}
		} else {
			stream = php_stream_open_wrapper(Z_STRVAL_P(zfc), "r", REPORT_ERRORS|USE_PATH, NULL);
		}

		if (!stream) {
			zval_ptr_dtor(&zfc);
			return FAILURE;
		} else {
			zval *znc = php_http_ztyp(IS_STRING, *zname), *ztc = php_http_ztyp(IS_STRING, *ztype);
			char *key = format_key(HASH_KEY_IS_STRING, Z_STRVAL_P(znc), 0, name);
			STATUS ret =  php_http_message_body_add_form_file(body, key, Z_STRVAL_P(ztc), Z_STRVAL_P(zfc), stream);

			efree(key);
			zval_ptr_dtor(&znc);
			zval_ptr_dtor(&ztc);
			zval_ptr_dtor(&zfc);
			if (!zdata || Z_TYPE_PP(zdata) != IS_RESOURCE) {
				php_stream_close(stream);
			}
			return ret;
		}

	}
}

struct splitbody_arg {
	php_http_buffer_t buf;
	php_http_message_parser_t *parser;
	char *boundary_str;
	size_t boundary_len;
	size_t consumed;
};

static size_t splitbody(void *opaque, char *buf, size_t len TSRMLS_DC)
{
	struct splitbody_arg *arg = opaque;
	const char *boundary = NULL;
	size_t consumed = 0;
	int first_boundary;

	do {
		first_boundary = !(consumed || arg->consumed);

		if ((boundary = php_http_locate_str(buf, len, arg->boundary_str + first_boundary, arg->boundary_len - first_boundary))) {
			size_t real_boundary_len = arg->boundary_len - 1, cut;
			const char *real_boundary = boundary + !first_boundary;
			int eol_len = 0;

			if (buf + len <= real_boundary + real_boundary_len) {
				/* if we just have enough data for the boundary, it's just a byte too less */
				arg->consumed += consumed;
				return consumed;
			}

			if (!first_boundary) {
				/* this is not the first boundary, read rest of this message */
				php_http_buffer_append(&arg->buf, buf, real_boundary - buf);
				php_http_message_parser_parse(arg->parser, &arg->buf, 0, &arg->parser->message);
			}

			/* move after the boundary */
			cut = real_boundary - buf + real_boundary_len;
			buf += cut;
			len -= cut;
			consumed += cut;

			if (buf == php_http_locate_bin_eol(buf, len, &eol_len)) {
				/* skip CRLF */
				buf += eol_len;
				len -= eol_len;
				consumed += eol_len;

				if (!first_boundary) {
					/* advance messages */
					php_http_message_t *msg;

					msg = php_http_message_init(NULL, 0 TSRMLS_CC);
					msg->parent = arg->parser->message;
					arg->parser->message = msg;
				}
			} else {
				/* is this the last boundary? */
				if (*buf == '-') {
					/* ignore the rest */
					consumed += len;
					len = 0;
				} else {
					/* let this be garbage */
					php_http_error(HE_WARNING, PHP_HTTP_E_MESSAGE_BODY, "Malformed multipart boundary at pos %zu", consumed);
					return -1;
				}
			}
		}
	} while (boundary && len);

	/* let there be room for the next boundary */
	if (len > arg->boundary_len) {
		consumed += len - arg->boundary_len;
		php_http_buffer_append(&arg->buf, buf, len - arg->boundary_len);
		php_http_message_parser_parse(arg->parser, &arg->buf, 0, &arg->parser->message);
	}

	arg->consumed += consumed;
	return consumed;
}

PHP_HTTP_API php_http_message_t *php_http_message_body_split(php_http_message_body_t *body, const char *boundary)
{
	php_stream *s = php_http_message_body_stream(body);
	php_http_buffer_t *tmp = NULL;
	php_http_message_t *msg = NULL;
	struct splitbody_arg arg;
	TSRMLS_FETCH_FROM_CTX(body->ts);

	php_http_buffer_init(&arg.buf);
	arg.parser = php_http_message_parser_init(NULL TSRMLS_CC);
	arg.boundary_len = spprintf(&arg.boundary_str, 0, "\n--%s", boundary);
	arg.consumed = 0;

	php_stream_rewind(s);
	while (!php_stream_eof(s)) {
		php_http_buffer_passthru(&tmp, 0x1000, (php_http_buffer_pass_func_t) _php_stream_read, s, splitbody, &arg TSRMLS_CC);
	}

	msg = arg.parser->message;
	arg.parser->message = NULL;

	php_http_buffer_free(&tmp);
	php_http_message_parser_free(&arg.parser);
	php_http_buffer_dtor(&arg.buf);
	STR_FREE(arg.boundary_str);

	return msg;
}

/* PHP */

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 			PHP_HTTP_BEGIN_ARGS_EX(HttpMessageBody, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)						PHP_HTTP_EMPTY_ARGS_EX(HttpMessageBody, method, 0)
#define PHP_HTTP_MESSAGE_BODY_ME(method, visibility)	PHP_ME(HttpMessageBody, method, PHP_HTTP_ARGS(HttpMessageBody, method), visibility)

PHP_HTTP_BEGIN_ARGS(__construct, 0)
	PHP_HTTP_ARG_VAL(stream, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(__toString);
PHP_HTTP_BEGIN_ARGS(unserialize, 1)
	PHP_HTTP_ARG_VAL(serialized, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(toStream, 1)
	PHP_HTTP_ARG_VAL(stream, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(toCallback, 1)
	PHP_HTTP_ARG_VAL(callback, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResource);

PHP_HTTP_BEGIN_ARGS(append, 1)
	PHP_HTTP_ARG_VAL(string, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addForm, 0)
	PHP_HTTP_ARG_ARR(fields, 1, 0)
	PHP_HTTP_ARG_ARR(files, 1, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addPart, 1)
	PHP_HTTP_ARG_OBJ(http\\Message, message, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(etag);

PHP_HTTP_BEGIN_ARGS(stat, 0)
	PHP_HTTP_ARG_VAL(what, 0)
PHP_HTTP_END_ARGS;

static zend_class_entry *php_http_message_body_class_entry;

zend_class_entry *php_http_message_body_get_class_entry(void)
{
	return php_http_message_body_class_entry;
}

static zend_function_entry php_http_message_body_method_entry[] = {
	PHP_HTTP_MESSAGE_BODY_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_MESSAGE_BODY_ME(__toString, ZEND_ACC_PUBLIC)
	PHP_MALIAS(HttpMessageBody, toString, __toString, args_for_HttpMessageBody___toString, ZEND_ACC_PUBLIC)
	PHP_MALIAS(HttpMessageBody, serialize, __toString, args_for_HttpMessageBody___toString, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(unserialize, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(toStream, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(toCallback, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(getResource, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(append, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(addForm, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(addPart, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(etag, ZEND_ACC_PUBLIC)
	PHP_HTTP_MESSAGE_BODY_ME(stat, ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers php_http_message_body_object_handlers;

PHP_MINIT_FUNCTION(http_message_body)
{
	PHP_HTTP_REGISTER_CLASS(http\\Message, Body, http_message_body, php_http_object_get_class_entry(), 0);
	php_http_message_body_class_entry->create_object = php_http_message_body_object_new;
	memcpy(&php_http_message_body_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_message_body_object_handlers.clone_obj = php_http_message_body_object_clone;
	zend_class_implements(php_http_message_body_class_entry TSRMLS_CC, 1, zend_ce_serializable);
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
#if PHP_VERSION_ID < 50339
	zend_hash_copy(((zend_object *) o)->properties, &(ce->default_properties), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval*));
#else
	object_properties_init((zend_object *) o, ce);
#endif

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

	with_error_handling(EH_THROW, php_http_exception_get_class_entry()) {
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

PHP_METHOD(HttpMessageBody, unserialize)
{
	char *us_str;
	int us_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &us_str, &us_len)) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_stream *s = php_stream_memory_open(0, us_str, us_len);

		obj->body = php_http_message_body_init(NULL, s TSRMLS_CC);
	}
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


PHP_METHOD(HttpMessageBody, toCallback)
{
	php_http_pass_fcall_arg_t fcd;
	long offset = 0, forlen = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "f|ll", &fcd.fci, &fcd.fcc, &offset, &forlen)) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		fcd.fcz = getThis();
		Z_ADDREF_P(fcd.fcz);
		TSRMLS_SET_CTX(fcd.ts);

		php_http_message_body_to_callback(obj->body, php_http_pass_fcall_callback, &fcd, offset, forlen);
		zend_fcall_info_args_clear(&fcd.fci, 1);

		zval_ptr_dtor(&fcd.fcz);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpMessageBody, getResource)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETVAL_RESOURCE(obj->body->stream_id);
	}
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

PHP_METHOD(HttpMessageBody, addForm)
{
	HashTable *fields = NULL, *files = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|h!h!", &fields, &files)) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		RETURN_SUCCESS(php_http_message_body_add_form(obj->body, fields, files));
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpMessageBody, addPart)
{
	zval *zobj;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &zobj, php_http_message_get_class_entry())) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_http_message_object_t *mobj = zend_object_store_get_object(zobj TSRMLS_CC);

		php_http_message_body_add_part(obj->body, mobj->message);
		RETURN_TRUE;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpMessageBody, etag)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		char *etag = php_http_message_body_etag(obj->body);

		if (etag) {
			RETURN_STRING(etag, 0);
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpMessageBody, stat)
{
	char *field_str = NULL;
	int field_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &field_str, &field_len)) {
		php_http_message_body_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		const php_stream_statbuf *sb = php_http_message_body_stat(obj->body);

		if (sb) {
			if (field_str && field_len) {
					switch (*field_str) {
						case 's':
						case 'S':
							RETURN_LONG(sb->sb.st_size);
							break;
						case 'a':
						case 'A':
							RETURN_LONG(sb->sb.st_atime);
							break;
						case 'm':
						case 'M':
							RETURN_LONG(sb->sb.st_mtime);
							break;
						case 'c':
						case 'C':
							RETURN_LONG(sb->sb.st_ctime);
							break;
						default:
							php_http_error(HE_WARNING, PHP_HTTP_E_MESSAGE_BODY, "unknown stat field: '%s' (should be one of [s]ize, [a]time, [m]time or [c]time)", field_str);
							break;
					}
			} else {
				array_init(return_value);
				add_assoc_long_ex(return_value, ZEND_STRS("size"), sb->sb.st_size);
				add_assoc_long_ex(return_value, ZEND_STRS("atime"), sb->sb.st_atime);
				add_assoc_long_ex(return_value, ZEND_STRS("mtime"), sb->sb.st_mtime);
				add_assoc_long_ex(return_value, ZEND_STRS("ctime"), sb->sb.st_ctime);
				return;
			}
		}
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
