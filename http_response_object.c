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

#include "php_http_std_defs.h"
#include "php_http_response_object.h"

#ifdef ZEND_ENGINE_2

#include "missing.h"

#define http_response_object_declare_default_properties() _http_response_object_declare_default_properties(TSRMLS_C)
static inline void _http_response_object_declare_default_properties(TSRMLS_D);

zend_class_entry *http_response_object_ce;
zend_function_entry http_response_object_fe[] = {
	PHP_ME(HttpResponse, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)

	PHP_ME(HttpResponse, setETag, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getETag, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setContentDisposition, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getContentDisposition, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setContentType, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getContentType, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setCache, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getCache, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setCacheControl, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getCacheControl, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setGzip, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getGzip, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setThrottleDelay, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getThrottleDelay, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setSendBuffersize, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getSendBuffersize, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getData, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setFile, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getFile, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, setStream, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(HttpResponse, getStream, NULL, ZEND_ACC_PUBLIC)

	PHP_ME(HttpResponse, send, NULL, ZEND_ACC_PUBLIC)

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

#endif /* ZEND_ENGINE_2 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

