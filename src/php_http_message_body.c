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

#include "ext/standard/php_lcg.h"

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

static ZEND_RESULT_CODE add_recursive_fields(php_http_message_body_t *body, const char *name, HashTable *fields);
static ZEND_RESULT_CODE add_recursive_files(php_http_message_body_t *body, const char *name, HashTable *files);

php_http_message_body_t *php_http_message_body_init(php_http_message_body_t **body_ptr, php_stream *stream)
{
	php_http_message_body_t *body;

	if (body_ptr && *body_ptr) {
		body = *body_ptr;
		php_http_message_body_addref(body);
		return body;
	}

	body = ecalloc(1, sizeof(php_http_message_body_t));
	body->refcount = 1;

	if (stream) {
		body->res = stream->res;
		GC_ADDREF(body->res);
	} else {
		body->res = php_stream_temp_create(TEMP_STREAM_DEFAULT, 0xffff)->res;
	}
	php_stream_auto_cleanup(php_http_message_body_stream(body));

	if (body_ptr) {
		*body_ptr = body;
	}

	return body;
}

unsigned php_http_message_body_addref(php_http_message_body_t *body)
{
	return ++body->refcount;
}

php_http_message_body_t *php_http_message_body_copy(php_http_message_body_t *from, php_http_message_body_t *to)
{
	if (from) {
		if (to) {
			php_stream_truncate_set_size(php_http_message_body_stream(to), 0);
		} else {
			to = php_http_message_body_init(NULL, NULL);
		}
		php_http_message_body_to_stream(from, php_http_message_body_stream(to), 0, 0);

		if (to->boundary) {
			efree(to->boundary);
		}
		if (from->boundary) {
			to->boundary = estrdup(from->boundary);
		}
	} else {
		to = NULL;
	}
	return to;
}

void php_http_message_body_free(php_http_message_body_t **body_ptr)
{
	if (*body_ptr) {
		php_http_message_body_t *body = *body_ptr;
		if (!--body->refcount) {
			zend_list_delete(body->res);
			body->res = NULL;
			PTR_FREE(body->boundary);
			efree(body);
		}
		*body_ptr = NULL;
	}
}

const php_stream_statbuf *php_http_message_body_stat(php_http_message_body_t *body)
{
	php_stream_stat(php_http_message_body_stream(body), &body->ssb);
	return &body->ssb;
}

const char *php_http_message_body_boundary(php_http_message_body_t *body)
{
	if (!body->boundary) {
		union { double dbl; int num[2]; } data;

		data.dbl = php_combined_lcg();
		spprintf(&body->boundary, 0, "%x.%x", data.num[0], data.num[1]);
	}
	return body->boundary;
}

char *php_http_message_body_etag(php_http_message_body_t *body)
{
	php_http_etag_t *etag;
	php_stream *s = php_http_message_body_stream(body);

	/* real file or temp buffer ? */
	if (s->ops != &php_stream_temp_ops && s->ops != &php_stream_memory_ops) {
		php_stream_stat(php_http_message_body_stream(body), &body->ssb);

		if (body->ssb.sb.st_mtime) {
			char *etag;

			spprintf(&etag, 0, "%lx-%lx-%lx", body->ssb.sb.st_ino, body->ssb.sb.st_mtime, body->ssb.sb.st_size);
			return etag;
		}
	}

	/* content based */
	if ((etag = php_http_etag_init(PHP_HTTP_G->env.etag_mode))) {
		php_http_message_body_to_callback(body, (php_http_pass_callback_t) php_http_etag_update, etag, 0, 0);
		return php_http_etag_finish(etag);
	}

	return NULL;
}

zend_string *php_http_message_body_to_string(php_http_message_body_t *body, off_t offset, size_t forlen)
{
	php_stream *s = php_http_message_body_stream(body);

	php_stream_seek(s, offset, SEEK_SET);
	if (!forlen) {
		forlen = -1;
	}
	return php_stream_copy_to_mem(s, forlen, 0);
}

ZEND_RESULT_CODE php_http_message_body_to_stream(php_http_message_body_t *body, php_stream *dst, off_t offset, size_t forlen)
{
	php_stream *s = php_http_message_body_stream(body);

	php_stream_seek(s, offset, SEEK_SET);

	if (!forlen) {
		forlen = -1;
	}
	return php_stream_copy_to_stream_ex(s, dst, forlen, NULL);
}

ZEND_RESULT_CODE php_http_message_body_to_callback(php_http_message_body_t *body, php_http_pass_callback_t cb, void *cb_arg, off_t offset, size_t forlen)
{
	php_stream *s = php_http_message_body_stream(body);
	char *buf = emalloc(0x1000);

	php_stream_seek(s, offset, SEEK_SET);

	if (!forlen) {
		forlen = -1;
	}
	while (!php_stream_eof(s)) {
		size_t read = php_stream_read(s, buf, MIN(forlen, 0x1000));

		if (read) {
			if (-1 == cb(cb_arg, buf, read)) {
				return FAILURE;
			}
		}

		if (read < MIN(forlen, sizeof(buf))) {
			break;
		}

		if (forlen && !(forlen -= read)) {
			break;
		}
	}
	efree(buf);

	return SUCCESS;
}

size_t php_http_message_body_append(php_http_message_body_t *body, const char *buf, size_t len)
{
	php_stream *s;
	size_t written;

	if (!(s = php_http_message_body_stream(body))) {
		return -1;
	}

	if (s->ops->seek) {
		php_stream_seek(s, 0, SEEK_END);
	}

	written = php_stream_write(s, buf, len);

	if (written != len) {
		php_error_docref(NULL, E_WARNING, "Failed to append %zu bytes to body; wrote %zu", len, written);
	}

	return len;
}

size_t php_http_message_body_appendf(php_http_message_body_t *body, const char *fmt, ...)
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

ZEND_RESULT_CODE php_http_message_body_add_form(php_http_message_body_t *body, HashTable *fields, HashTable *files)
{
	if (fields) {
		if (SUCCESS != add_recursive_fields(body, NULL, fields)) {
			return FAILURE;
		}
	}
	if (files) {
		if (SUCCESS != add_recursive_files(body, NULL, files)) {
			return FAILURE;
		}
	}

	return SUCCESS;
}

void php_http_message_body_add_part(php_http_message_body_t *body, php_http_message_t *part)
{
	BOUNDARY_OPEN(body);
	php_http_message_to_callback(part, (php_http_pass_callback_t) php_http_message_body_append, body);
	BOUNDARY_CLOSE(body);
}


ZEND_RESULT_CODE php_http_message_body_add_form_field(php_http_message_body_t *body, const char *name, const char *value_str, size_t value_len)
{
	zend_string *safe_name, *zstr_name = zend_string_init(name, strlen(name), 0);

#if PHP_VERSION_ID < 70300
	safe_name = php_addslashes(zstr_name, 1);
#else
	safe_name = php_addslashes(zstr_name);
	zend_string_release_ex(zstr_name, 0);
#endif

	BOUNDARY_OPEN(body);
	php_http_message_body_appendf(
		body,
		"Content-Disposition: form-data; name=\"%s\"" PHP_HTTP_CRLF
		"" PHP_HTTP_CRLF,
		safe_name->val
	);
	php_http_message_body_append(body, value_str, value_len);
	BOUNDARY_CLOSE(body);

	zend_string_release(safe_name);
	return SUCCESS;
}

ZEND_RESULT_CODE php_http_message_body_add_form_file(php_http_message_body_t *body, const char *name, const char *ctype, const char *path, php_stream *in)
{
	size_t path_len = strlen(path);
	char *path_dup = estrndup(path, path_len);
	zend_string *base_name, *safe_name, *zstr_name = zend_string_init(name, strlen(name), 0);

#if PHP_VERSION_ID < 70300
	safe_name = php_addslashes(zstr_name, 1);
#else
	safe_name = php_addslashes(zstr_name);
	zend_string_release_ex(zstr_name, 0);
#endif
	base_name = php_basename(path_dup, path_len, NULL, 0);

	BOUNDARY_OPEN(body);
	php_http_message_body_appendf(
		body,
		"Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"" PHP_HTTP_CRLF
		"Content-Transfer-Encoding: binary" PHP_HTTP_CRLF
		"Content-Type: %s" PHP_HTTP_CRLF
		PHP_HTTP_CRLF,
		safe_name->val, base_name->val, ctype
	);
	php_stream_copy_to_stream_ex(in, php_http_message_body_stream(body), PHP_STREAM_COPY_ALL, NULL);
	BOUNDARY_CLOSE(body);

	zend_string_release(safe_name);
	zend_string_release(base_name);
	efree(path_dup);

	return SUCCESS;
}

static inline char *format_key(php_http_arrkey_t *key, const char *prefix) {
	char *new_key = NULL;

	if (prefix && *prefix) {
		if (key->key) {
			spprintf(&new_key, 0, "%s[%s]", prefix, key->key->val);
		} else {
			spprintf(&new_key, 0, "%s[%lu]", prefix, key->h);
		}
	} else if (key->key) {
		new_key = estrdup(key->key->val);
	} else {
		new_key = estrdup("");
	}

	return new_key;
}

static ZEND_RESULT_CODE add_recursive_field_value(php_http_message_body_t *body, const char *name, zval *value)
{
	zend_string *zs = zval_get_string(value);
	ZEND_RESULT_CODE rc = php_http_message_body_add_form_field(body, name, zs->val, zs->len);
	zend_string_release(zs);
	return rc;
}

static ZEND_RESULT_CODE add_recursive_fields(php_http_message_body_t *body, const char *name, HashTable *fields)
{
	zval *val;
	php_http_arrkey_t key;

	if (!HT_IS_RECURSIVE(fields)) {
		HT_PROTECT_RECURSION(fields);
		ZEND_HASH_FOREACH_KEY_VAL_IND(fields, key.h, key.key, val)
		{
			char *str = format_key(&key, name);

			if (Z_TYPE_P(val) != IS_ARRAY && Z_TYPE_P(val) != IS_OBJECT) {
				if (SUCCESS != add_recursive_field_value(body, str, val)) {
					efree(str);
					HT_UNPROTECT_RECURSION(fields);
					return FAILURE;
				}
			} else if (SUCCESS != add_recursive_fields(body, str, HASH_OF(val))) {
				efree(str);
				HT_UNPROTECT_RECURSION(fields);
				return FAILURE;
			}
			efree(str);
		}
		ZEND_HASH_FOREACH_END();
		HT_UNPROTECT_RECURSION(fields);
	}

	return SUCCESS;
}

static ZEND_RESULT_CODE add_recursive_files(php_http_message_body_t *body, const char *name, HashTable *files)
{
	zval *zdata = NULL, *zfile, *zname, *ztype;

	/* single entry */
	if (!(zname = zend_hash_str_find(files, ZEND_STRL("name")))
	||	!(ztype = zend_hash_str_find(files, ZEND_STRL("type")))
	||	!(zfile = zend_hash_str_find(files, ZEND_STRL("file")))
	) {
		zval *val;
		php_http_arrkey_t key;

		if (!HT_IS_RECURSIVE(files)) {
			HT_PROTECT_RECURSION(files);
			ZEND_HASH_FOREACH_KEY_VAL_IND(files, key.h, key.key, val)
			{
				if (Z_TYPE_P(val) == IS_ARRAY || Z_TYPE_P(val) == IS_OBJECT) {
					char *str = format_key(&key, name);

					if (SUCCESS != add_recursive_files(body, str, HASH_OF(val))) {
						efree(str);
						HT_UNPROTECT_RECURSION(files);
						return FAILURE;
					}
					efree(str);
				}
			}
			ZEND_HASH_FOREACH_END();
			HT_UNPROTECT_RECURSION(files);
		}
		return SUCCESS;
	} else {
		/* stream entry */
		php_stream *stream;
		zend_string *zfc = zval_get_string(zfile);

		if ((zdata = zend_hash_str_find(files, ZEND_STRL("data")))) {
			if (Z_TYPE_P(zdata) == IS_RESOURCE) {
				php_stream_from_zval_no_verify(stream, zdata);
			} else {
				zend_string *tmp = zval_get_string(zdata);

				stream = php_stream_memory_open(TEMP_STREAM_READONLY, tmp->val, tmp->len);
				zend_string_release(tmp);
			}
		} else {
			stream = php_stream_open_wrapper(zfc->val, "r", REPORT_ERRORS|USE_PATH, NULL);
		}

		if (!stream) {
			zend_string_release(zfc);
			return FAILURE;
		} else {
			zend_string *znc = zval_get_string(zname), *ztc = zval_get_string(ztype);
			php_http_arrkey_t arrkey = {0, znc};
			char *key = format_key(&arrkey, name);
			ZEND_RESULT_CODE ret = php_http_message_body_add_form_file(body, key, ztc->val, zfc->val, stream);

			efree(key);
			zend_string_release(znc);
			zend_string_release(ztc);
			zend_string_release(zfc);
			if (!zdata || Z_TYPE_P(zdata) != IS_RESOURCE) {
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

static size_t splitbody(void *opaque, char *buf, size_t len)
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

					msg = php_http_message_init(NULL, 0, NULL);
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
					php_error_docref(NULL, E_WARNING, "Malformed multipart boundary at pos %zu", consumed);
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

php_http_message_t *php_http_message_body_split(php_http_message_body_t *body, const char *boundary)
{
	php_stream *s = php_http_message_body_stream(body);
	php_http_buffer_t *tmp = NULL;
	php_http_message_t *msg = NULL;
	struct splitbody_arg arg;

	php_http_buffer_init(&arg.buf);
	arg.parser = php_http_message_parser_init(NULL);
	arg.boundary_len = spprintf(&arg.boundary_str, 0, "\n--%s", boundary);
	arg.consumed = 0;

	php_stream_rewind(s);
	while (!php_stream_eof(s)) {
		php_http_buffer_passthru(&tmp, 0x1000, (php_http_buffer_pass_func_t) _php_stream_read, s, splitbody, &arg);
	}

	msg = arg.parser->message;
	arg.parser->message = NULL;

	php_http_buffer_free(&tmp);
	php_http_message_parser_free(&arg.parser);
	php_http_buffer_dtor(&arg.buf);
	PTR_FREE(arg.boundary_str);

	return msg;
}

static zend_class_entry *php_http_message_body_class_entry;
zend_class_entry *php_http_get_message_body_class_entry(void)
{
	return php_http_message_body_class_entry;
}

static zend_object_handlers php_http_message_body_object_handlers;

zend_object *php_http_message_body_object_new(zend_class_entry *ce)
{
	return &php_http_message_body_object_new_ex(ce, NULL)->zo;
}

php_http_message_body_object_t *php_http_message_body_object_new_ex(zend_class_entry *ce, php_http_message_body_t *body)
{
	php_http_message_body_object_t *o;

	o = ecalloc(1, sizeof(*o) + zend_object_properties_size(ce));
	zend_object_std_init(&o->zo, php_http_message_body_class_entry);
	object_properties_init(&o->zo, ce);

	o->gc = emalloc(sizeof(zval));

	if (body) {
		o->body = body;
	}

	o->zo.handlers = &php_http_message_body_object_handlers;

	return o;
}

zend_object *php_http_message_body_object_clone(zval *object)
{
	php_http_message_body_object_t *new_obj;
	php_http_message_body_object_t *old_obj = PHP_HTTP_OBJ(NULL, object);
	php_http_message_body_t *body = php_http_message_body_copy(old_obj->body, NULL);

	new_obj = php_http_message_body_object_new_ex(old_obj->zo.ce, body);
	zend_objects_clone_members(&new_obj->zo, &old_obj->zo);

	return &new_obj->zo;
}

static HashTable *php_http_message_body_object_get_gc(zval *object, zval **table, int *n)
{
	php_http_message_body_object_t *obj = PHP_HTTP_OBJ(NULL, object);
	HashTable *props = Z_OBJPROP_P(object);
	uint32_t count = zend_hash_num_elements(props);

	obj->gc = erealloc(obj->gc, (1 + count) * sizeof(zval));

	if (php_http_message_body_stream(obj->body)) {
		*n = 1;
		php_stream_to_zval(php_http_message_body_stream(obj->body), obj->gc);
	} else {
		*n = 0;
	}

	if (count) {
		zval *val;

		ZEND_HASH_FOREACH_VAL(props, val)
		{
			ZVAL_COPY_VALUE(&obj->gc[(*n)++], val);
		}
		ZEND_HASH_FOREACH_END();
	}
	*table = obj->gc;

	return NULL;
}

void php_http_message_body_object_free(zend_object *object)
{
	php_http_message_body_object_t *obj = PHP_HTTP_OBJ(object, NULL);

	PTR_FREE(obj->gc);
	php_http_message_body_free(&obj->body);
	zend_object_std_dtor(object);
}

#define PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj) \
	do { \
		if (!obj->body) { \
			obj->body = php_http_message_body_init(NULL, NULL); \
			php_stream_to_zval(php_http_message_body_stream(obj->body), obj->gc); \
		} \
	} while(0)

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, stream)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, __construct)
{
	php_http_message_body_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
	zval *zstream = NULL;
	php_stream *stream;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|r!", &zstream), invalid_arg, return);

	if (zstream) {
		php_http_expect(php_stream_from_zval_no_verify(stream, zstream), unexpected_val, return);

		if (obj->body) {
			php_http_message_body_free(&obj->body);
		}
		obj->body = php_http_message_body_init(NULL, stream);
		php_stream_to_zval(stream, obj->gc);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody___toString, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, __toString)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_body_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		zend_string *zs;

		PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

		zs = php_http_message_body_to_string(obj->body, 0, 0);
		if (zs) {
			RETURN_STR(zs);
		}
	}
	RETURN_EMPTY_STRING();
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_unserialize, 0, 0, 1)
	ZEND_ARG_INFO(0, serialized)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, unserialize)
{
	char *us_str;
	size_t us_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &us_str, &us_len)) {
		php_http_message_body_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		php_stream *s = php_stream_memory_open(0, us_str, us_len);

		obj->body = php_http_message_body_init(NULL, s);
		php_stream_to_zval(s, obj->gc);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_toStream, 0, 0, 1)
	ZEND_ARG_INFO(0, stream)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, maxlen)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, toStream)
{
	zval *zstream;
	zend_long offset = 0, forlen = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "r|ll", &zstream, &offset, &forlen)) {
		php_stream *stream;
		php_http_message_body_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

		php_stream_from_zval(stream, zstream);
		php_http_message_body_to_stream(obj->body, stream, offset, forlen);
		RETURN_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_toCallback, 0, 0, 1)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, maxlen)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, toCallback)
{
	php_http_pass_fcall_arg_t fcd;
	zend_long offset = 0, forlen = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "f|ll", &fcd.fci, &fcd.fcc, &offset, &forlen)) {
		php_http_message_body_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

		ZVAL_COPY(&fcd.fcz, getThis());
		php_http_message_body_to_callback(obj->body, php_http_pass_fcall_callback, &fcd, offset, forlen);
		zend_fcall_info_args_clear(&fcd.fci, 1);
		zval_ptr_dtor(&fcd.fcz);
		RETURN_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_getResource, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, getResource)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_body_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

		php_stream_to_zval(php_http_message_body_stream(obj->body), return_value);
		Z_ADDREF_P(return_value);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_getBoundary, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, getBoundary)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_body_object_t * obj = PHP_HTTP_OBJ(NULL, getThis());

		PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

		if (obj->body->boundary) {
			RETURN_STRING(obj->body->boundary);
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_append, 0, 0, 1)
	ZEND_ARG_INFO(0, string)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, append)
{
	char *str;
	size_t len;
	php_http_message_body_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &str, &len), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

	php_http_expect(len == php_http_message_body_append(obj->body, str, len), runtime, return);

	RETURN_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_addForm, 0, 0, 0)
	ZEND_ARG_ARRAY_INFO(0, fields, 1)
	ZEND_ARG_ARRAY_INFO(0, files, 1)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, addForm)
{
	HashTable *fields = NULL, *files = NULL;
	php_http_message_body_object_t *obj;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|h!h!", &fields, &files), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

	php_http_expect(SUCCESS == php_http_message_body_add_form(obj->body, fields, files), runtime, return);

	RETURN_ZVAL(getThis(), 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_addPart, 0, 0, 1)
	ZEND_ARG_OBJ_INFO(0, message, http\\Message, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, addPart)
{
	zval *zobj;
	php_http_message_body_object_t *obj;
	php_http_message_object_t *mobj;
	zend_error_handling zeh;

	php_http_expect(SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "O", &zobj, php_http_message_get_class_entry()), invalid_arg, return);

	obj = PHP_HTTP_OBJ(NULL, getThis());
	mobj = PHP_HTTP_OBJ(NULL, zobj);

	PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

	zend_replace_error_handling(EH_THROW, php_http_get_exception_runtime_class_entry(), &zeh);
	php_http_message_body_add_part(obj->body, mobj->message);
	zend_restore_error_handling(&zeh);

	if (!EG(exception)) {
		RETURN_ZVAL(getThis(), 1, 0);
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_etag, 0, 0, 0)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, etag)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_message_body_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		char *etag;

		PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

		if ((etag = php_http_message_body_etag(obj->body))) {
			RETURN_STR(php_http_cs2zs(etag, strlen(etag)));
		} else {
			RETURN_FALSE;
		}
	}
}

ZEND_BEGIN_ARG_INFO_EX(ai_HttpMessageBody_stat, 0, 0, 0)
	ZEND_ARG_INFO(0, field)
ZEND_END_ARG_INFO();
PHP_METHOD(HttpMessageBody, stat)
{
	char *field_str = NULL;
	size_t field_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS(), "|s", &field_str, &field_len)) {
		php_http_message_body_object_t *obj = PHP_HTTP_OBJ(NULL, getThis());
		const php_stream_statbuf *sb;

		PHP_HTTP_MESSAGE_BODY_OBJECT_INIT(obj);

		if ((sb = php_http_message_body_stat(obj->body))) {
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
							php_error_docref(NULL, E_WARNING, "Unknown stat field: '%s' (should be one of [s]ize, [a]time, [m]time or [c]time)", field_str);
							break;
					}
			} else {
				object_init(return_value);
				add_property_long_ex(return_value, ZEND_STRL("size"), sb->sb.st_size);
				add_property_long_ex(return_value, ZEND_STRL("atime"), sb->sb.st_atime);
				add_property_long_ex(return_value, ZEND_STRL("mtime"), sb->sb.st_mtime);
				add_property_long_ex(return_value, ZEND_STRL("ctime"), sb->sb.st_ctime);
			}
		}
	}
}

static zend_function_entry php_http_message_body_methods[] = {
	PHP_ME(HttpMessageBody, __construct,  ai_HttpMessageBody___construct,  ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(HttpMessageBody, __toString,   ai_HttpMessageBody___toString,   ZEND_ACC_PUBLIC)
	PHP_MALIAS(HttpMessageBody, toString, __toString, ai_HttpMessageBody___toString, ZEND_ACC_PUBLIC)
	PHP_MALIAS(HttpMessageBody, serialize, __toString, ai_HttpMessageBody___toString, ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, unserialize,  ai_HttpMessageBody_unserialize,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, toStream,     ai_HttpMessageBody_toStream,     ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, toCallback,   ai_HttpMessageBody_toCallback,   ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, getResource,  ai_HttpMessageBody_getResource,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, getBoundary,  ai_HttpMessageBody_getBoundary,  ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, append,       ai_HttpMessageBody_append,       ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, addForm,      ai_HttpMessageBody_addForm,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, addPart,      ai_HttpMessageBody_addPart,      ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, etag,         ai_HttpMessageBody_etag,         ZEND_ACC_PUBLIC)
	PHP_ME(HttpMessageBody, stat,         ai_HttpMessageBody_stat,         ZEND_ACC_PUBLIC)
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_message_body)
{
	zend_class_entry ce = {0};

	INIT_NS_CLASS_ENTRY(ce, "http\\Message", "Body", php_http_message_body_methods);
	php_http_message_body_class_entry = zend_register_internal_class(&ce);
	php_http_message_body_class_entry->create_object = php_http_message_body_object_new;
	memcpy(&php_http_message_body_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_message_body_object_handlers.offset = XtOffsetOf(php_http_message_body_object_t, zo);
	php_http_message_body_object_handlers.clone_obj = php_http_message_body_object_clone;
	php_http_message_body_object_handlers.free_obj = php_http_message_body_object_free;
	php_http_message_body_object_handlers.get_gc = php_http_message_body_object_get_gc;
	zend_class_implements(php_http_message_body_class_entry, 1, zend_ce_serializable);

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
