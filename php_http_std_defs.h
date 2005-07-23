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

#if defined(PHP_WIN32)
#	if defined(HTTP_EXPORTS)
#		define PHP_HTTP_API __declspec(dllexport)
#	elif defined(COMPILE_DL_HTTP)
#		define PHP_HTTP_API __declspec(dllimport)
#	else
#		define PHP_HTTP_API
#	endif
#else
#	define PHP_HTTP_API
#endif

/* make functions that return SUCCESS|FAILURE more obvious */
typedef int STATUS;

/* lenof() */
#define lenof(S) (sizeof(S) - 1)

/* STR_SET() */
#define STR_SET(target, source) \
	if(target) efree(target); \
	target = source

/* return bool (v == SUCCESS) */
#define RETVAL_SUCCESS(v) RETVAL_BOOL(SUCCESS == (v))
#define RETURN_SUCCESS(v) RETURN_BOOL(SUCCESS == (v))
/* return object(values) */
#define RETVAL_OBJECT(o) \
	RETVAL_OBJVAL((o)->value.obj)
#define RETURN_OBJECT(o) \
	RETVAL_OBJECT(o); \
	return
#define RETVAL_OBJVAL(ov) \
	return_value->is_ref = 1; \
	return_value->type = IS_OBJECT; \
	return_value->value.obj = (ov); \
	zval_add_ref(&return_value); \
	zend_objects_store_add_ref(return_value TSRMLS_CC)
#define RETURN_OBJVAL(ov) \
	RETVAL_OBJVAL(ov); \
	return

/* function accepts no args */
#define NO_ARGS \
	if (ZEND_NUM_ARGS()) { \
		zend_error(E_NOTICE, "Wrong parameter count for %s()", get_active_function_name(TSRMLS_C)); \
	}

/* check if return value is used */
#define IF_RETVAL_USED \
	if (!return_value_used) { \
		return; \
	} else

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

/* known methods */
#define HTTP_KNOWN_METHODS \
		/* HTTP 1.1 */ \
		"GET, HEAD, POST, PUT, DELETE, OPTIONS, TRACE, CONNECT, " \
		/* WebDAV - RFC 2518 */ \
		"PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK, " \
		/* WebDAV Versioning - RFC 3253 */ \
		"VERSION-CONTROL, REPORT, CHECKOUT, CHECKIN, UNCHECKOUT, " \
		"MKWORKSPACE, UPDATE, LABEL, MERGE, BASELINE-CONTROL, MKACTIVITY, " \
		/* WebDAV Access Control - RFC 3744 */ \
		"ACL, " \
		/* END */


/* server vars shorthand */
#define HTTP_SERVER_VARS Z_ARRVAL_P(PG(http_globals)[TRACK_VARS_SERVER])

#define HTTP_PHP_INI_ENTRY(entry, default, scope, updater, global) \
	STD_PHP_INI_ENTRY(entry, default, scope, updater, global, zend_http_globals, http_globals)

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
#define array_append(src, dst)	\
	{ \
		ulong idx; \
		uint klen; \
		char *key = NULL; \
		zval **data; \
		 \
		for (	zend_hash_internal_pointer_reset(Z_ARRVAL_P(src)); \
				zend_hash_get_current_key_ex(Z_ARRVAL_P(src), &key, &klen, &idx, 0, NULL) != HASH_KEY_NON_EXISTANT && \
				zend_hash_get_current_data(Z_ARRVAL_P(src), (void **) &data) == SUCCESS; \
				zend_hash_move_forward(Z_ARRVAL_P(src))) \
		{ \
			if (key) { \
				zval **tmp; \
				 \
				if (SUCCESS == zend_hash_find(Z_ARRVAL_P(dst), key, klen, (void **) &tmp)) { \
					if (Z_TYPE_PP(tmp) != IS_ARRAY) { \
						convert_to_array_ex(tmp); \
					} \
					add_next_index_zval(*tmp, *data); \
				} else { \
					add_assoc_zval(dst, key, *data); \
				} \
				zval_add_ref(data); \
				key = NULL; \
			} \
		} \
	}
/* }}} */

#define HTTP_LONG_CONSTANT(name, const) REGISTER_LONG_CONSTANT(name, const, CONST_CS | CONST_PERSISTENT)

/* {{{ objects & properties */
#ifdef ZEND_ENGINE_2

#	define HTTP_STATIC_ME_ALIAS(me, al, ai) ZEND_FENTRY(me, ZEND_FN(al), ai, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)

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
#	define OBJ_PROP(o) (o)->zo.properties
#	define DCL_STATIC_PROP(a, t, n, v) zend_declare_property_ ##t(ce, (#n), sizeof(#n), (v), (ZEND_ACC_ ##a | ZEND_ACC_STATIC) TSRMLS_CC)
#	define DCL_STATIC_PROP_Z(a, n, v) zend_declare_property(ce, (#n), sizeof(#n), (v), (ZEND_ACC_ ##a | ZEND_ACC_STATIC) TSRMLS_CC)
#	define DCL_STATIC_PROP_N(a, n) zend_declare_property_null(ce, (#n), sizeof(#n), (ZEND_ACC_ ##a | ZEND_ACC_STATIC) TSRMLS_CC)
#	define GET_STATIC_PROP_EX(ce, n) zend_std_get_static_property(ce, (#n), sizeof(#n), 0 TSRMLS_CC)
#ifdef zend_update_class_constants
#	define USE_STATIC_PROP_EX(ce) zend_update_class_constants(ce TSRMLS_CC)
#else
#	define USE_STATIC_PROP_EX(ce)
#endif
#	define SET_STATIC_PROP_EX(ce, n, v) \
	{ \
		int refcount; \
		zend_uchar is_ref; \
		zval **__static = GET_STATIC_PROP_EX(ce, n); \
 \
		refcount = (*__static)->refcount; \
		is_ref = (*__static)->is_ref; \
		switch (Z_TYPE_PP(__static)) \
		{ \
			case IS_BOOL: case IS_LONG: case IS_NULL: \
			break; \
			case IS_RESOURCE: \
				zend_list_delete(Z_LVAL_PP(__static)); \
			break; \
			case IS_STRING: case IS_CONSTANT: \
				free(Z_STRVAL_PP(__static)); \
			break; \
			case IS_OBJECT: \
				Z_OBJ_HT_PP(__static)->del_ref(*__static TSRMLS_CC); \
			break; \
			case IS_ARRAY: case IS_CONSTANT_ARRAY: \
				if (Z_ARRVAL_PP(__static) && Z_ARRVAL_PP(__static) != &EG(symbol_table)) { \
					zend_hash_destroy(Z_ARRVAL_PP(__static)); \
					free(Z_ARRVAL_PP(__static)); \
				} \
			break; \
		} \
		**__static = *(v); \
		switch (Z_TYPE_PP(__static)) \
		{ \
			case IS_BOOL: case IS_LONG: case IS_NULL: \
			break; \
			case IS_RESOURCE: \
				zend_list_addref(Z_LVAL_PP(__static)); \
			break; \
			case IS_STRING: case IS_CONSTANT: \
				Z_STRVAL_PP(__static) = (char *) zend_strndup(Z_STRVAL_PP(__static), Z_STRLEN_PP(__static)); \
			break; \
			case IS_OBJECT: \
			{ \
				Z_OBJ_HT_PP(__static)->add_ref(*__static TSRMLS_CC); \
			} \
			break; \
			case IS_ARRAY: case IS_CONSTANT_ARRAY: \
			{ \
				if (Z_ARRVAL_PP(__static) != &EG(symbol_table)) { \
					zval *tmp; \
					HashTable *old = Z_ARRVAL_PP(__static); \
					Z_ARRVAL_PP(__static) = (HashTable *) malloc(sizeof(HashTable)); \
					zend_hash_init(Z_ARRVAL_PP(__static), 0, NULL, ZVAL_PTR_DTOR, 0); \
					zend_hash_copy(Z_ARRVAL_PP(__static), old, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *)); \
				} \
			} \
			break; \
		} \
		(*__static)->refcount = refcount; \
		(*__static)->is_ref = is_ref; \
	}
#define SET_STATIC_PROP_STRING_EX(ce, n, s, d) \
	{ \
		zval *__tmp; \
		MAKE_STD_ZVAL(__tmp); \
		ZVAL_STRING(__tmp, (s), (d)); \
		SET_STATIC_PROP_EX(ce, n, __tmp); \
		zval_dtor(__tmp); \
		efree(__tmp); \
	}
#define SET_STATIC_PROP_STRINGL_EX(ce, n, s, l, d) \
	{ \
		zval *__tmp; \
		MAKE_STD_ZVAL(__tmp); \
		ZVAL_STRINGL(__tmp, (s), (l), (d)); \
		SET_STATIC_PROP_EX(ce, n, __tmp); \
		zval_dtor(__tmp); \
		efree(__tmp); \
	}
#	define DCL_PROP(a, t, n, v) zend_declare_property_ ##t(ce, (#n), sizeof(#n), (v), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define DCL_PROP_Z(a, n, v) zend_declare_property(ce, (#n), sizeof(#n), (v), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define DCL_PROP_N(a, n) zend_declare_property_null(ce, (#n), sizeof(#n), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define UPD_PROP(o, t, n, v) UPD_PROP_EX(o, getThis(), t, n, v)
#	define UPD_PROP_EX(o, this, t, n, v) zend_update_property_ ##t(o->zo.ce, this, (#n), sizeof(#n), (v) TSRMLS_CC)
#	define SET_PROP(o, n, z) SET_PROP_EX(o, getThis(), n, z)
#	define SET_PROP_EX(o, this, n, z) zend_update_property(o->zo.ce, this, (#n), sizeof(#n), (z) TSRMLS_CC)
#	define GET_PROP(o, n) GET_PROP_EX(o, getThis(), n)
#	define GET_PROP_EX(o, this, n) zend_read_property(o->zo.ce, this, (#n), sizeof(#n), 0 TSRMLS_CC)

#	define ACC_PROP_PRIVATE(ce, flags)		((flags & ZEND_ACC_PRIVATE) && (EG(scope) && ce == EG(scope))
#	define ACC_PROP_PROTECTED(ce, flags)	((flags & ZEND_ACC_PROTECTED) && (zend_check_protected(ce, EG(scope))))
#	define ACC_PROP_PUBLIC(flags)			(flags & ZEND_ACC_PUBLIC)
#	define ACC_PROP(ce, flags)				(ACC_PROP_PUBLIC(flags) || ACC_PROP_PRIVATE(ce, flags) || ACC_PROP_PROTECTED(ce, flags))

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
#define HTTP_E_MSG			8L

#ifdef ZEND_ENGINE_2
#	define HTTP_BEGIN_ARGS_EX(class, method, ret_ref, req_args)	static ZEND_BEGIN_ARG_INFO_EX(args_for_ ##class## _ ##method , 0, ret_ref, req_args)
#	define HTTP_BEGIN_ARGS_AR(class, method, ret_ref, req_args)	static ZEND_BEGIN_ARG_INFO_EX(args_for_ ##class## _ ##method , 1, ret_ref, req_args)
#	define HTTP_END_ARGS										}
#	define HTTP_EMPTY_ARGS_EX(class, method, ret_ref)			HTTP_BEGIN_ARGS_EX(class, method, ret_ref, 0) HTTP_END_ARGS
#	define HTTP_ARGS(class, method)								args_for_ ##class## _ ##method
#	define HTTP_ARG_VAL(name, pass_ref)							ZEND_ARG_INFO(pass_ref, name)
#	define HTTP_ARG_OBJ(class, name, allow_null)				ZEND_ARG_OBJ_INFO(1, name, class, allow_null)
#endif

#ifdef HTTP_HAVE_CURL
#	ifdef ZEND_ENGINE_2
#		define HTTP_DECLARE_ARG_PASS_INFO() \
			static \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_3, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO(); \
 \
			static \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_4, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO(); \
 \
			static \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_5, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO(); \
 \
			static \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_all, 1) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO()
#	else
#		define HTTP_DECLARE_ARG_PASS_INFO() \
			static unsigned char http_arg_pass_ref_3[] = {3, BYREF_NONE, BYREF_NONE, BYREF_FORCE}; \
			static unsigned char http_arg_pass_ref_4[] = {4, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_FORCE}; \
			static unsigned char http_arg_pass_ref_5[] = {5, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_FORCE}; \
			static unsigned char http_arg_pass_ref_all[]={1, BYREF_FORCE_REST}
#	endif /* ZEND_ENGINE_2 */
#else
#	define HTTP_DECLARE_ARG_PASS_INFO()
#endif /* HTTP_HAVE_CURL */


#ifndef TSRMLS_FETCH_FROM_CTX
#	ifdef ZTS
#		define TSRMLS_FETCH_FROM_CTX(ctx)	void ***tsrm_ls = (void ***) ctx
#	else
#		define TSRMLS_FETCH_FROM_CTX(ctx)
#	endif
#endif

#ifndef TSRMLS_SET_CTX
#	ifdef ZTS
#		define TSRMLS_SET_CTX(ctx)	(void ***) ctx = tsrm_ls
#	else
#		define TSRMLS_SET_CTX(ctx)
#	endif
#endif



#endif /* PHP_HTTP_STD_DEFS_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

