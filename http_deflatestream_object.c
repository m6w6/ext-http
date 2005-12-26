/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2005, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */


#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#define HTTP_WANT_ZLIB
#include "php_http.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_ZLIB)

#include "php_http_api.h"
#include "php_http_encoding_api.h"
#include "php_http_exception_object.h"
#include "php_http_deflatestream_object.h"

#define HTTP_BEGIN_ARGS(method, ret_ref, req_args) 	HTTP_BEGIN_ARGS_EX(HttpDeflateStream, method, ret_ref, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)			HTTP_EMPTY_ARGS_EX(HttpDeflateStream, method, ret_ref)
#define HTTP_DEFLATE_ME(method, visibility)			PHP_ME(HttpDeflateStream, method, HTTP_ARGS(HttpDeflateStream, method), visibility)

HTTP_BEGIN_ARGS(__construct, 0, 0)
	HTTP_ARG_VAL(flags, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(update, 0, 1)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(finish, 0);

#define http_deflatestream_object_declare_default_properties() _http_deflatestream_object_declare_default_properties(TSRMLS_C)
static inline void _http_deflatestream_object_declare_default_properties(TSRMLS_D);

zend_class_entry *http_deflatestream_object_ce;
zend_function_entry http_deflatestream_object_fe[] = {
	HTTP_DEFLATE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_DEFLATE_ME(update, ZEND_ACC_PUBLIC)
	HTTP_DEFLATE_ME(finish, ZEND_ACC_PUBLIC)
	
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_deflatestream_object_handlers;

PHP_MINIT_FUNCTION(http_deflatestream_object)
{
	HTTP_REGISTER_CLASS_EX(HttpDeflateStream, http_deflatestream_object, NULL, 0);
	http_deflatestream_object_handlers.clone_obj = _http_deflatestream_object_clone_obj;
	return SUCCESS;
}

zend_object_value _http_deflatestream_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_deflatestream_object_new_ex(ce, NULL, NULL);
}

zend_object_value _http_deflatestream_object_new_ex(zend_class_entry *ce, http_encoding_stream *s, http_deflatestream_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_deflatestream_object *o;

	o = ecalloc(1, sizeof(http_deflatestream_object));
	o->zo.ce = ce;
	
	if (ptr) {
		*ptr = o;
	}

	if (s) {
		o->stream = s;
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);

	ov.handle = putObject(http_deflatestream_object, o);
	ov.handlers = &http_deflatestream_object_handlers;

	return ov;
}

zend_object_value _http_deflatestream_object_clone_obj(zval *this_ptr TSRMLS_DC)
{
	http_encoding_stream *s;
	getObject(http_deflatestream_object, obj);
	
	s = ecalloc(1, sizeof(http_encoding_stream));
	s->flags = obj->stream->flags;
	deflateCopy(&s->stream, &obj->stream->stream);
	s->stream.opaque = phpstr_dup(s->stream.opaque);
	
	return http_deflatestream_object_new_ex(Z_OBJCE_P(this_ptr), s, NULL);
}

static inline void _http_deflatestream_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_deflatestream_object_ce;

#ifndef WONKY
	DCL_CONST(long, "TYPE_GZIP", HTTP_DEFLATE_TYPE_GZIP);
	DCL_CONST(long, "TYPE_ZLIB", HTTP_DEFLATE_TYPE_ZLIB);
	DCL_CONST(long, "TYPE_RAW", HTTP_DEFLATE_TYPE_RAW);
	DCL_CONST(long, "LEVEL_DEF", HTTP_DEFLATE_LEVEL_DEF);
	DCL_CONST(long, "LEVEL_MIN", HTTP_DEFLATE_LEVEL_MIN);
	DCL_CONST(long, "LEVEL_MAX", HTTP_DEFLATE_LEVEL_MAX);
#endif
}

void _http_deflatestream_object_free(zend_object *object TSRMLS_DC)
{
	http_deflatestream_object *o = (http_deflatestream_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	if (o->stream) {
		http_encoding_deflate_stream_free(&o->stream);
	}
	efree(o);
}

/* {{{ proto void HttpDeflateStream::__construct([int flags = 0])
 *
 * Creates a new HttpDeflateStream object instance.
 * 
 * Accepts an optional int parameter specifying how to initialize the deflate stream.
 */ 
PHP_METHOD(HttpDeflateStream, __construct)
{
	long flags = 0;
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &flags)) {
		getObject(http_deflatestream_object, obj);
		
		if (!obj->stream) {
			obj->stream = http_encoding_deflate_stream_init(NULL, flags);
		} else {
			http_error_ex(HE_WARNING, HTTP_E_ENCODING, "HttpDeflateStream cannot be initialized twice");
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto string HttpDeflateStream::update(string data)
 *
 * Passes more data through the deflate stream.
 * 
 * Expects a string parameter containing (a part of) the data to deflate.
 * 
 * Returns deflated data on success or FALSE on failure.
 */
PHP_METHOD(HttpDeflateStream, update)
{
	int data_len;
	size_t encoded_len = 0;
	char *data, *encoded = NULL;
	getObject(http_deflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!obj->stream) {
		if (!(obj->stream = http_encoding_deflate_stream_init(NULL, 0))) {
			RETURN_FALSE;
		}
	}
	
	if (SUCCESS == http_encoding_deflate_stream_update(obj->stream, data, data_len, &encoded, &encoded_len)) {
		RETURN_STRINGL(encoded, encoded_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string HttpDeflateStream::finish()
 *
 * Finalizes the deflate stream.  The deflate stream can be reused after finalizing.
 * 
 * Returns the final part of deflated data.
 */ 
PHP_METHOD(HttpDeflateStream, finish)
{
	size_t encoded_len = 0;
	char *encoded = NULL;
	getObject(http_deflatestream_object, obj);
	
	NO_ARGS;
	
	if (!obj->stream) {
		RETURN_FALSE;
	}
	
	if (SUCCESS == http_encoding_deflate_stream_finish(obj->stream, &encoded, &encoded_len)) {
		RETVAL_STRINGL(encoded, encoded_len, 0);
	} else {
		RETVAL_FALSE;
	}
	
	http_encoding_deflate_stream_dtor(obj->stream);
	http_encoding_deflate_stream_init(obj->stream, obj->stream->flags);
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

