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

#ifndef PHP_HTTP_STD_DEFS_H
#define PHP_HTTP_STD_DEFS_H

#ifdef PHP_WIN32
#	define PHP_HTTP_API __declspec(dllexport)
#else
#	define PHP_HTTP_API
#endif

/* make functions that return SUCCESS|FAILURE more obvious */
typedef int STATUS;

/* lenof() */
#define lenof(S) (sizeof(S) - 1)

/* return bool (v == SUCCESS) */
#define RETURN_SUCCESS(v) RETURN_BOOL(SUCCESS == (v))

/* function accepts no args */
#define NO_ARGS \
	if (ZEND_NUM_ARGS()) { \
		zend_error(E_NOTICE, "Wrong parameter count for %s()", get_active_function_name(TSRMLS_C)); \
	}

/* CR LF */
#define HTTP_CRLF "\r\n"

/* default cache control */
#define HTTP_DEFAULT_CACHECONTROL "private, must-revalidate, max-age=0"

/* max URL length */
#define HTTP_URL_MAXLEN 2048
#define HTTP_URI_MAXLEN HTTP_URL_MAXLEN

/* def URL arg separator */
#define HTTP_URL_ARGSEP "&"
#define HTTP_URI_ARGSEP HTTP_URL_ARGSEP

/* send buffer size */
#define HTTP_SENDBUF_SIZE 2097152

/* CURL buffer size */
#define HTTP_CURLBUF_SIZE 16384

/* server vars shorthand */
#define HTTP_SERVER_VARS Z_ARRVAL_P(PG(http_globals)[TRACK_VARS_SERVER])

/* {{{ arrays */
#define FOREACH_VAL(array, val) FOREACH_HASH_VAL(Z_ARRVAL_P(array), val)
#define FOREACH_HASH_VAL(hash, val) \
	for (	zend_hash_internal_pointer_reset(hash); \
			zend_hash_get_current_data(hash, (void **) &val) == SUCCESS; \
			zend_hash_move_forward(hash))

#define FOREACH_KEY(array, strkey, numkey) FOREACH_HASH_KEY(Z_ARRVAL_P(array), strkey, numkey)
#define FOREACH_HASH_KEY(hash, strkey, numkey) \
	for (	zend_hash_internal_pointer_reset(hash); \
			zend_hash_get_current_key(hash, &strkey, &numkey, 0) != HASH_KEY_NON_EXISTANT; \
			zend_hash_move_forward(hash)) \

#define FOREACH_KEYVAL(array, strkey, numkey, val) FOREACH_HASH_KEYVAL(Z_ARRVAL_P(array), strkey, numkey, val)
#define FOREACH_HASH_KEYVAL(hash, strkey, numkey, val) \
	for (	zend_hash_internal_pointer_reset(hash); \
			zend_hash_get_current_key(hash, &strkey, &numkey, 0) != HASH_KEY_NON_EXISTANT && \
			zend_hash_get_current_data(hash, (void **) &val) == SUCCESS; \
			zend_hash_move_forward(hash)) \

#define array_copy(src, dst)	zend_hash_copy(Z_ARRVAL_P(dst), Z_ARRVAL_P(src), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *))
#define array_merge(src, dst)	zend_hash_merge(Z_ARRVAL_P(dst), Z_ARRVAL_P(src), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *), 1)
/* }}} */

#define HTTP_LONG_CONSTANT(name, const) REGISTER_LONG_CONSTANT(name, const, CONST_CS | CONST_PERSISTENT);

/* {{{ objects & properties */
#ifdef ZEND_ENGINE_2


#	define HTTP_REGISTER_CLASS_EX(classname, name, parent, flags) \
	{ \
		zend_class_entry ce; \
		INIT_CLASS_ENTRY(ce, #classname, name## _fe); \
		ce.create_object = name## _new; \
		name## _ce = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
		name## _ce->ce_flags |= flags;  \
		memcpy(& name## _handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers)); \
		name## _declare_default_properties(); \
	}

#	define HTTP_REGISTER_CLASS(classname, name, parent, flags) \
	{ \
		zend_class_entry ce; \
		INIT_CLASS_ENTRY(ce, #classname, name## _fe); \
		ce.create_object = NULL; \
		name## _ce = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
		name## _ce->ce_flags |= flags;  \
	}

#	define getObject(t, o) getObjectEx(t, o, getThis())
#	define getObjectEx(t, o, v) t * o = ((t *) zend_object_store_get_object(v TSRMLS_CC))
#	define OBJ_PROP(o) o->zo.properties
#	define DCL_PROP(a, t, n, v) zend_declare_property_ ##t(ce, (#n), sizeof(#n), (v), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define DCL_PROP_Z(a, n, v) zend_declare_property(ce, (#n), sizeof(#n), (v), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define DCL_PROP_N(a, n) zend_declare_property_null(ce, (#n), sizeof(#n), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define UPD_PROP(o, t, n, v) zend_update_property_ ##t(o->zo.ce, getThis(), (#n), sizeof(#n), (v) TSRMLS_CC)
#	define SET_PROP(o, n, z) zend_update_property(o->zo.ce, getThis(), (#n), sizeof(#n), (z) TSRMLS_CC)
#	define GET_PROP(o, n) zend_read_property(o->zo.ce, getThis(), (#n), sizeof(#n), 0 TSRMLS_CC)

#	define INIT_PARR(o, n) \
	{ \
		zval *__tmp; \
		MAKE_STD_ZVAL(__tmp); \
		array_init(__tmp); \
		SET_PROP(o, n, __tmp); \
	}

#	define FREE_PARR(o, p) \
	{ \
		zval *__tmp = NULL; \
		if (__tmp = GET_PROP(o, p)) { \
			zval_dtor(__tmp); \
			FREE_ZVAL(__tmp); \
			__tmp = NULL; \
		} \
	}

#	define SET_EH_THROW() SET_EH_THROW_EX(zend_exception_get_default())
#	define SET_EH_THROW_HTTP() SET_EH_THROW_EX(http_exception_get_default())
#	define SET_EH_THROW_EX(ex) php_set_error_handling(EH_THROW, ex TSRMLS_CC)
#	define SET_EH_NORMAL() php_set_error_handling(EH_NORMAL, NULL TSRMLS_CC)

#endif /* ZEND_ENGINE_2 */
/* }}} */

#ifndef E_THROW
#	define E_THROW 0
#endif

#define HTTP_E_UNKOWN		0L
#define HTTP_E_PARSE		1L
#define HTTP_E_HEADER		2L
#define HTTP_E_OBUFFER		3L
#define HTTP_E_CURL			4L
#define HTTP_E_ENCODE		5L
#define HTTP_E_PARAM		6L
#define HTTP_E_URL			7L

#endif /* PHP_HTTP_STD_DEFS_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

