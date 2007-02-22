/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2007, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_ZLIB
#include "php_http.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_ZLIB)

#include "php_http_api.h"
#include "php_http_encoding_api.h"
#include "php_http_exception_object.h"
#include "php_http_inflatestream_object.h"

#define HTTP_BEGIN_ARGS(method, req_args) 	HTTP_BEGIN_ARGS_EX(HttpInflateStream, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method)				HTTP_EMPTY_ARGS_EX(HttpInflateStream, method, 0)
#define HTTP_INFLATE_ME(method, visibility)	PHP_ME(HttpInflateStream, method, HTTP_ARGS(HttpInflateStream, method), visibility)

HTTP_BEGIN_ARGS(__construct, 0)
	HTTP_ARG_VAL(flags, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(factory, 0)
	HTTP_ARG_VAL(flags, 0)
	HTTP_ARG_VAL(class_name, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(update, 1)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(flush, 0)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(finish, 0)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

#define THIS_CE http_inflatestream_object_ce
zend_class_entry *http_inflatestream_object_ce;
zend_function_entry http_inflatestream_object_fe[] = {
	HTTP_INFLATE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_INFLATE_ME(update, ZEND_ACC_PUBLIC)
	HTTP_INFLATE_ME(flush, ZEND_ACC_PUBLIC)
	HTTP_INFLATE_ME(finish, ZEND_ACC_PUBLIC)
	
	HTTP_INFLATE_ME(factory, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_inflatestream_object_handlers;

PHP_MINIT_FUNCTION(http_inflatestream_object)
{
	HTTP_REGISTER_CLASS_EX(HttpInflateStream, http_inflatestream_object, NULL, 0);
	http_inflatestream_object_handlers.clone_obj = _http_inflatestream_object_clone_obj;
	
#ifndef WONKY
	zend_declare_class_constant_long(THIS_CE, ZEND_STRS("FLUSH_NONE")-1, HTTP_ENCODING_STREAM_FLUSH_NONE TSRMLS_CC);
	zend_declare_class_constant_long(THIS_CE, ZEND_STRS("FLUSH_SYNC")-1, HTTP_ENCODING_STREAM_FLUSH_SYNC TSRMLS_CC);
	zend_declare_class_constant_long(THIS_CE, ZEND_STRS("FLUSH_FULL")-1, HTTP_ENCODING_STREAM_FLUSH_FULL TSRMLS_CC);
#endif
	
	return SUCCESS;
}

zend_object_value _http_inflatestream_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_inflatestream_object_new_ex(ce, NULL, NULL);
}

zend_object_value _http_inflatestream_object_new_ex(zend_class_entry *ce, http_encoding_stream *s, http_inflatestream_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_inflatestream_object *o;

	o = ecalloc(1, sizeof(http_inflatestream_object));
	o->zo.ce = ce;
	
	if (ptr) {
		*ptr = o;
	}

	if (s) {
		o->stream = s;
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), zend_hash_num_elements(&ce->default_properties), NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = putObject(http_inflatestream_object, o);
	ov.handlers = &http_inflatestream_object_handlers;

	return ov;
}

zend_object_value _http_inflatestream_object_clone_obj(zval *this_ptr TSRMLS_DC)
{
	http_encoding_stream *s;
	zend_object_value new_ov;
	http_inflatestream_object *new_obj = NULL;
	getObject(http_inflatestream_object, old_obj);
	
	s = ecalloc(1, sizeof(http_encoding_stream));
	s->flags = old_obj->stream->flags;
	inflateCopy(&s->stream, &old_obj->stream->stream);
	s->stream.opaque = phpstr_dup(s->stream.opaque);
	
	new_ov = http_inflatestream_object_new_ex(old_obj->zo.ce, s, &new_obj);
	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);
	
	return new_ov;
}

void _http_inflatestream_object_free(zend_object *object TSRMLS_DC)
{
	http_inflatestream_object *o = (http_inflatestream_object *) object;

	if (o->stream) {
		http_encoding_inflate_stream_free(&o->stream);
	}
	freeObject(o);
}

/* {{{ proto void HttpInflateStream::__construct([int flags = 0])
	Creates a new HttpInflateStream object instance. */
PHP_METHOD(HttpInflateStream, __construct)
{
	long flags = 0;
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &flags)) {
		getObject(http_inflatestream_object, obj);
		
		if (!obj->stream) {
			obj->stream = http_encoding_inflate_stream_init(NULL, flags & 0x0fffffff);
		} else {
			http_error_ex(HE_WARNING, HTTP_E_ENCODING, "HttpInflateStream cannot be initialized twice");
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto HttpInflateStream HttpInflateStream::factory([int flags[, string class = "HttpInflateStream"]])
	Creates a new HttpInflateStream object instance. */
PHP_METHOD(HttpInflateStream, factory)
{
	long flags = 0;
	char *cn = NULL;
	int cl = 0;
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|ls", &flags, &cn, &cl)) {
		zend_object_value ov;
		http_encoding_stream *s = http_encoding_inflate_stream_init(NULL, flags & 0x0fffffff);
		
		if (SUCCESS == http_object_new(&ov, cn, cl, _http_inflatestream_object_new_ex, http_inflatestream_object_ce, s, NULL)) {
			RETVAL_OBJVAL(ov, 0);
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto string HttpInflateStream::update(string data)
	Passes more data through the inflate stream. */
PHP_METHOD(HttpInflateStream, update)
{
	int data_len;
	size_t decoded_len = 0;
	char *data, *decoded = NULL;
	getObject(http_inflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!data_len) {
		RETURN_STRING("", 1);
	}
	
	if (!obj->stream && !(obj->stream = http_encoding_inflate_stream_init(NULL, 0))) {
		RETURN_FALSE;
	}
	
	if (SUCCESS == http_encoding_inflate_stream_update(obj->stream, data, data_len, &decoded, &decoded_len)) {
		RETURN_STRINGL(decoded, decoded_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string HttpInflateStream::flush([string data])
	Flush the inflate stream. */
PHP_METHOD(HttpInflateStream, flush)
{
	int data_len = 0;
	size_t decoded_len = 0;
	char *decoded = NULL, *data = NULL;
	getObject(http_inflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!obj->stream && !(obj->stream = http_encoding_inflate_stream_init(NULL, 0))) {
		RETURN_FALSE;
	}
	
	/* flushing the inflate stream is a no-op */
	if (!data_len) {
		RETURN_STRINGL("", 0, 1);
	} else if (SUCCESS == http_encoding_inflate_stream_update(obj->stream, data, data_len, &decoded, &decoded_len)) {
		RETURN_STRINGL(decoded, decoded_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string HttpInflateStream::finish([string data])
	Finalizes the inflate stream.  The inflate stream can be reused after finalizing. */
PHP_METHOD(HttpInflateStream, finish)
{
	int data_len = 0;
	size_t updated_len = 0, decoded_len = 0;
	char *updated = NULL, *decoded = NULL, *data = NULL;
	getObject(http_inflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!obj->stream && !(obj->stream = http_encoding_inflate_stream_init(NULL, 0))) {
		RETURN_FALSE;
	}
	
	if (data_len) {
		if (SUCCESS != http_encoding_inflate_stream_update(obj->stream, data, data_len, &updated, &updated_len)) {
			RETURN_FALSE;
		}
	}
	
	if (SUCCESS == http_encoding_inflate_stream_finish(obj->stream, &decoded, &decoded_len)) {
		if (updated_len) {
			updated = erealloc(updated, updated_len + decoded_len + 1);
			updated[updated_len + decoded_len] = '\0';
			memcpy(updated + updated_len, decoded, decoded_len);
			STR_FREE(decoded);
			updated_len += decoded_len;
			RETVAL_STRINGL(updated, updated_len, 0);
		} else {
			STR_FREE(updated);
			RETVAL_STRINGL(decoded, decoded_len, 0);
		}
	} else {
		STR_FREE(updated);
		RETVAL_FALSE;
	}
	
	http_encoding_inflate_stream_dtor(obj->stream);
	http_encoding_inflate_stream_init(obj->stream, obj->stream->flags);
}
/* }}} */

#endif /* ZEND_ENGINE_2 && HTTP_HAVE_ZLIB*/

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

