/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */


#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "php.h"

#ifdef ZEND_ENGINE_2

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_std_defs.h"
#include "php_http_response_object.h"
#include "php_http_send_api.h"
#include "php_http_cache_api.h"

#include "missing.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

#define HTTP_BEGIN_ARGS(method, req_args) 		HTTP_BEGIN_ARGS_EX(HttpResponse, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)		HTTP_EMPTY_ARGS_EX(HttpResponse, method, ret_ref)
#define HTTP_RESPONSE_ME(method, visibility)	PHP_ME(HttpResponse, method, HTTP_ARGS(HttpResponse, method), visibility)

HTTP_EMPTY_ARGS(__destruct, 0);
HTTP_BEGIN_ARGS(__construct, 0)
	HTTP_ARG_VAL(cache, 0)
	HTTP_ARG_VAL(gzip, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getETag, 0);
HTTP_BEGIN_ARGS(setETag, 1)
	HTTP_ARG_VAL(etag, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getCache, 0);
HTTP_BEGIN_ARGS(setCache, 1)
	HTTP_ARG_VAL(cache, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getGzip, 0);
HTTP_BEGIN_ARGS(setGzip, 1)
	HTTP_ARG_VAL(gzip, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getCacheControl, 0);
HTTP_BEGIN_ARGS(setCacheControl, 1)
	HTTP_ARG_VAL(cache_control, 0)
	HTTP_ARG_VAL(raw, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getContentType, 0);
HTTP_BEGIN_ARGS(setContentType, 1)
	HTTP_ARG_VAL(content_type, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getContentDisposition, 0);
HTTP_BEGIN_ARGS(setContentDisposition, 1)
	HTTP_ARG_VAL(filename, 0)
	HTTP_ARG_VAL(send_inline, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getThrottleDelay, 0);
HTTP_BEGIN_ARGS(setThrottleDelay, 1)
	HTTP_ARG_VAL(seconds, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getSendBuffersize, 0);
HTTP_BEGIN_ARGS(setSendBuffersize, 1)
	HTTP_ARG_VAL(bytes, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getData, 0);
HTTP_BEGIN_ARGS(setData, 1)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getStream, 0);
HTTP_BEGIN_ARGS(setStream, 1)
	HTTP_ARG_VAL(stream, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getFile, 0);
HTTP_BEGIN_ARGS(setFile, 1)
	HTTP_ARG_VAL(filepath, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(send, 0)
	HTTP_ARG_VAL(clean_ob, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(catchOutput, 0)
	HTTP_ARG_VAL(clean_ob, 0)
HTTP_END_ARGS;

#define http_response_object_declare_default_properties() _http_response_object_declare_default_properties(TSRMLS_C)
static inline void _http_response_object_declare_default_properties(TSRMLS_D);

zend_class_entry *http_response_object_ce;
zend_function_entry http_response_object_fe[] = {
	HTTP_RESPONSE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_RESPONSE_ME(__destruct, ZEND_ACC_PUBLIC|ZEND_ACC_DTOR)

	HTTP_RESPONSE_ME(setETag, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getETag, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setContentDisposition, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getContentDisposition, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setContentType, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getContentType, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setCache, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getCache, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setCacheControl, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getCacheControl, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setGzip, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getGzip, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setThrottleDelay, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getThrottleDelay, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setSendBuffersize, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getSendBuffersize, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setData, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getData, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setFile, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getFile, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(setStream, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(getStream, ZEND_ACC_PUBLIC)

	HTTP_RESPONSE_ME(send, ZEND_ACC_PUBLIC)
	HTTP_RESPONSE_ME(catchOutput, ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL}
};
static zend_object_handlers http_response_object_handlers;

void _http_response_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS_EX(HttpResponse, http_response_object, NULL, 0);
}

zend_object_value _http_response_object_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value ov;
	http_response_object *o;

	o = ecalloc(1, sizeof(http_response_object));
	o->zo.ce = ce;

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = zend_objects_store_put(o, (zend_objects_store_dtor_t) zend_objects_destroy_object, http_response_object_free, NULL TSRMLS_CC);
	ov.handlers = &http_response_object_handlers;

	return ov;
}

static inline void _http_response_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_response_object_ce;

	DCL_PROP(PROTECTED, string, contentType, "application/x-octetstream");
	DCL_PROP(PROTECTED, string, eTag, "");
	DCL_PROP(PROTECTED, string, dispoFile, "");
	DCL_PROP(PROTECTED, string, cacheControl, "public");
	DCL_PROP(PROTECTED, string, data, "");
	DCL_PROP(PROTECTED, string, file, "");
	DCL_PROP(PROTECTED, long, stream, 0);
	DCL_PROP(PROTECTED, long, lastModified, 0);
	DCL_PROP(PROTECTED, long, dispoInline, 0);
	DCL_PROP(PROTECTED, long, cache, 0);
	DCL_PROP(PROTECTED, long, gzip, 0);
	DCL_PROP(PROTECTED, long, sendBuffersize, HTTP_SENDBUF_SIZE);
	DCL_PROP(PROTECTED, double, throttleDelay, 0.0);

	DCL_PROP(PRIVATE, long, raw_cache_header, 0);
	DCL_PROP(PRIVATE, long, send_mode, -1);
	DCL_PROP(PRIVATE, long, sent, 0);
	DCL_PROP(PRIVATE, long, catch_ob, 0);
}

void _http_response_object_free(zend_object *object TSRMLS_DC)
{
	http_response_object *o = (http_response_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	efree(o);
}

void _http_response_object_sendhandler(zval *this_ptr, http_response_object *obj, zend_bool clean_ob, zval *return_value TSRMLS_DC)
{
	zval *do_cache, *do_gzip;

	if (!obj) {
		getObject(http_response_object, o);
		obj = o;
	}

	do_cache = GET_PROP(obj, cache);
	do_gzip  = GET_PROP(obj, gzip);

	if (clean_ob) {
		/* interrupt on-the-fly etag generation */
		HTTP_G(etag).started = 0;
		/* discard previous output buffers */
		php_end_ob_buffers(0 TSRMLS_CC);
	}

	/* gzip */
	if (Z_LVAL_P(do_gzip)) {
		php_start_ob_buffer_named("ob_gzhandler", 0, 1 TSRMLS_CC);
	}

	/* caching */
	if (Z_LVAL_P(do_cache)) {
		char *cc_hdr;
		int cc_len;
		zval *cctrl, *etag, *lmod, *ccraw;

		etag  = GET_PROP(obj, eTag);
		lmod  = GET_PROP(obj, lastModified);
		cctrl = GET_PROP(obj, cacheControl);
		ccraw = GET_PROP(obj, raw_cache_header);

		if (Z_LVAL_P(ccraw)) {
			cc_hdr = Z_STRVAL_P(cctrl);
			cc_len = Z_STRLEN_P(cctrl);
		} else {
			char cc_header[42] = {0};
			sprintf(cc_header, "%s, must-revalidate, max-age=0", Z_STRVAL_P(cctrl));
			cc_hdr = cc_header;
			cc_len = Z_STRLEN_P(cctrl) + lenof(", must-revalidate, max-age=0");
		}

		http_cache_etag(Z_STRVAL_P(etag), Z_STRLEN_P(etag), cc_hdr, cc_len);
		http_cache_last_modified(Z_LVAL_P(lmod), Z_LVAL_P(lmod) ? Z_LVAL_P(lmod) : time(NULL), cc_hdr, cc_len);
	}

	/* content type */
	{
		zval *ctype = GET_PROP(obj, contentType);
		if (Z_STRLEN_P(ctype)) {
			http_send_content_type(Z_STRVAL_P(ctype), Z_STRLEN_P(ctype));
		} else {
			http_send_content_type("application/x-octetstream", lenof("application/x-octetstream"));
		}
	}

	/* content disposition */
	{
		zval *dispo_file = GET_PROP(obj, dispoFile);
		if (Z_STRLEN_P(dispo_file)) {
			zval *dispo_inline = GET_PROP(obj, dispoInline);
			http_send_content_disposition(Z_STRVAL_P(dispo_file), Z_STRLEN_P(dispo_file), (zend_bool) Z_LVAL_P(dispo_inline));
		}
	}

	/* throttling */
	{
		zval *send_buffersize, *throttle_delay;
		send_buffersize = GET_PROP(obj, sendBuffersize);
		throttle_delay  = GET_PROP(obj, throttleDelay);
		HTTP_G(send).buffer_size    = Z_LVAL_P(send_buffersize);
		HTTP_G(send).throttle_delay = Z_DVAL_P(throttle_delay);
	}

	/* send */
	{
		zval *send_mode = GET_PROP(obj, send_mode);
		switch (Z_LVAL_P(send_mode))
		{
			case SEND_DATA:
			{
				zval *zdata = GET_PROP(obj, data);
				RETURN_SUCCESS(http_send_data(Z_STRVAL_P(zdata), Z_STRLEN_P(zdata)));
			}

			case SEND_RSRC:
			{
				php_stream *the_real_stream;
				zval *the_stream = GET_PROP(obj, stream);
				php_stream_from_zval(the_real_stream, &the_stream);
				RETURN_SUCCESS(http_send_stream(the_real_stream));
			}

			default:
			{
				zval *zfile = GET_PROP(obj, file);
				RETURN_SUCCESS(http_send_file(Z_STRVAL_P(zfile)));
			}
		}
	}
}

#endif /* ZEND_ENGINE_2 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

