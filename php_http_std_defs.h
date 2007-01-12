/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2006, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
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

#ifndef MIN
#	define MIN(a,b) (a<b?a:b)
#endif
#ifndef MAX
#	define MAX(a,b) (a>b?a:b)
#endif

/* STR_SET() */
#ifndef STR_SET
#	define STR_SET(STR, SET) \
	{ \
		STR_FREE(STR); \
		STR = SET; \
	}
#endif

#define STR_PTR(s) (s?s:"")

#define INIT_ZARR(zv, ht) \
	{ \
		INIT_PZVAL(&(zv)); \
		Z_TYPE(zv) = IS_ARRAY; \
		Z_ARRVAL(zv) = (ht); \
	}

/* return bool (v == SUCCESS) */
#define RETVAL_SUCCESS(v) RETVAL_BOOL(SUCCESS == (v))
#define RETURN_SUCCESS(v) RETURN_BOOL(SUCCESS == (v))
/* return object(values) */
#define RETVAL_OBJECT(o, addref) \
	RETVAL_OBJVAL((o)->value.obj, addref)
#define RETURN_OBJECT(o, addref) \
	RETVAL_OBJECT(o, addref); \
	return
#define RETVAL_OBJVAL(ov, addref) \
	ZVAL_OBJVAL(return_value, ov, addref)
#define RETURN_OBJVAL(ov, addref) \
	RETVAL_OBJVAL(ov, addref); \
	return
#define ZVAL_OBJVAL(zv, ov, addref) \
	(zv)->type = IS_OBJECT; \
	(zv)->value.obj = (ov);\
	if (addref && Z_OBJ_HT_P(zv)->add_ref) { \
		Z_OBJ_HT_P(zv)->add_ref((zv) TSRMLS_CC); \
	}
/* return property */
#define RETVAL_PROP(n) RETVAL_PROP_EX(getThis(), n)
#define RETURN_PROP(n) RETURN_PROP_EX(getThis(), n)
#define RETVAL_PROP_EX(this, n) \
	{ \
		zval *__prop = GET_PROP_EX(this, n); \
		RETVAL_ZVAL(__prop, 1, 0); \
	}
#define RETURN_PROP_EX(this, n) \
	{ \
		zval *__prop = GET_PROP_EX(this, n); \
		RETURN_ZVAL(__prop, 1, 0); \
	}

/* function accepts no args */
#define NO_ARGS zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "");

/* CR LF */
#define HTTP_CRLF "\r\n"

/* default cache control */
#define HTTP_DEFAULT_CACHECONTROL "private, must-revalidate, max-age=0"

/* max URL length */
#define HTTP_URL_MAXLEN 4096

/* def URL arg separator */
#define HTTP_URL_ARGSEP "&"

/* send buffer size */
#define HTTP_SENDBUF_SIZE 8000 /*40960*/

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

#ifdef ZEND_ENGINE_2
#	include "ext/standard/file.h"
#	define HTTP_DEFAULT_STREAM_CONTEXT FG(default_context)
#else
#	define HTTP_DEFAULT_STREAM_CONTEXT NULL
#endif

#define HTTP_PHP_INI_ENTRY(entry, default, scope, updater, global) \
	STD_PHP_INI_ENTRY(entry, default, scope, updater, global, zend_http_globals, http_globals)
#define HTTP_PHP_INI_ENTRY_EX(entry, default, scope, updater, displayer, global) \
	STD_PHP_INI_ENTRY_EX(entry, default, scope, updater, global, zend_http_globals, http_globals, displayer)


#define HTTP_LONG_CONSTANT(name, const) REGISTER_LONG_CONSTANT(name, const, CONST_CS | CONST_PERSISTENT)

/* {{{ objects & properties */
#ifdef ZEND_ENGINE_2

#	define HTTP_STATIC_ME_ALIAS(me, al, ai) ZEND_FENTRY(me, ZEND_FN(al), ai, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)

#	define HTTP_REGISTER_CLASS_EX(classname, name, parent, flags) \
	{ \
		zend_class_entry ce; \
		memset(&ce, 0, sizeof(zend_class_entry)); \
		INIT_CLASS_ENTRY(ce, #classname, name## _fe); \
		ce.create_object = _ ##name## _new; \
		name## _ce = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
		name## _ce->ce_flags |= flags;  \
		memcpy(& name## _handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers)); \
	}

#	define HTTP_REGISTER_CLASS(classname, name, parent, flags) \
	{ \
		zend_class_entry ce; \
		memset(&ce, 0, sizeof(zend_class_entry)); \
		INIT_CLASS_ENTRY(ce, #classname, name## _fe); \
		ce.create_object = NULL; \
		name## _ce = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
		name## _ce->ce_flags |= flags;  \
	}

#	define HTTP_REGISTER_EXCEPTION(classname, cename, parent) \
	{ \
		zend_class_entry ce; \
		memset(&ce, 0, sizeof(zend_class_entry)); \
		INIT_CLASS_ENTRY(ce, #classname, NULL); \
		ce.create_object = NULL; \
		cename = zend_register_internal_class_ex(&ce, parent, NULL TSRMLS_CC); \
	}

#	define getObject(t, o) getObjectEx(t, o, getThis())
#	define getObjectEx(t, o, v) t * o = ((t *) zend_object_store_get_object(v TSRMLS_CC))
#	define putObject(t, o) zend_objects_store_put(o, (zend_objects_store_dtor_t) zend_objects_destroy_object, (zend_objects_free_object_storage_t) _ ##t## _free, NULL TSRMLS_CC);
#	ifndef WONKY
#		define freeObject(o) \
			if (OBJ_GUARDS(o)) { \
				zend_hash_destroy(OBJ_GUARDS(o)); \
				FREE_HASHTABLE(OBJ_GUARDS(o)); \
			} \
			if (OBJ_PROP(o)) { \
				zend_hash_destroy(OBJ_PROP(o)); \
				FREE_HASHTABLE(OBJ_PROP(o)); \
			} \
			efree(o);
#	else
#		define freeObject(o) \
			if (OBJ_PROP(o)) { \
				zend_hash_destroy(OBJ_PROP(o)); \
				FREE_HASHTABLE(OBJ_PROP(o)); \
			} \
			efree(o);
#	endif
#	define OBJ_PROP(o) (o)->zo.properties
#	define OBJ_GUARDS(o) (o)->zo.guards

#	define DCL_STATIC_PROP(a, t, n, v)		zend_declare_property_ ##t(OBJ_PROP_CE, (#n), sizeof(#n)-1, (v), (ZEND_ACC_ ##a | ZEND_ACC_STATIC) TSRMLS_CC)
#	define DCL_STATIC_PROP_Z(a, n, v)		zend_declare_property(OBJ_PROP_CE, (#n), sizeof(#n)-1, (v), (ZEND_ACC_ ##a | ZEND_ACC_STATIC) TSRMLS_CC)
#	define DCL_STATIC_PROP_N(a, n)			zend_declare_property_null(OBJ_PROP_CE, (#n), sizeof(#n)-1, (ZEND_ACC_ ##a | ZEND_ACC_STATIC) TSRMLS_CC)
#	define GET_STATIC_PROP_EX(ce, n)		zend_std_get_static_property(ce, (#n), sizeof(#n)-1, 0 TSRMLS_CC)
#	define UPD_STATIC_PROP_EX(ce, t, n, v)	zend_update_static_property_ ##t(ce, #n, sizeof(#n)-1, (v) TSRMLS_CC)
#	define UPD_STATIC_STRL_EX(ce, n, v, l)	zend_update_static_property_stringl(ce, #n, sizeof(#n)-1, (v), (l) TSRMLS_CC)
#	define SET_STATIC_PROP_EX(ce, n, v)		zend_update_static_property(ce, #n, sizeof(#n)-1, v TSRMLS_CC)
#	define GET_STATIC_PROP(n)				*GET_STATIC_PROP_EX(OBJ_PROP_CE, n)
#	define UPD_STATIC_PROP(t, n, v)			UPD_STATIC_PROP_EX(OBJ_PROP_CE, t, n, v)
#	define SET_STATIC_PROP(n, v)			SET_STATIC_PROP_EX(OBJ_PROP_CE, n, v)
#	define UPD_STATIC_STRL(n, v, l)			UPD_STATIC_STRL_EX(OBJ_PROP_CE, n, v, l)

#	define DCL_PROP(a, t, n, v)				zend_declare_property_ ##t(OBJ_PROP_CE, (#n), sizeof(#n)-1, (v), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define DCL_PROP_Z(a, n, v)				zend_declare_property(OBJ_PROP_CE, (#n), sizeof(#n)-1, (v), (ZEND_ACC_ ##a) TSRMLS_CC)
#	define DCL_PROP_N(a, n)					zend_declare_property_null(OBJ_PROP_CE, (#n), sizeof(#n)-1, (ZEND_ACC_ ##a) TSRMLS_CC)
#	define UPD_PROP(t, n, v)				UPD_PROP_EX(getThis(), t, n, v)
#	define UPD_PROP_EX(this, t, n, v)		zend_update_property_ ##t(OBJ_PROP_CE, this, (#n), sizeof(#n)-1, (v) TSRMLS_CC)
#	define UPD_STRL(n, v, l)				zend_update_property_stringl(OBJ_PROP_CE, getThis(), (#n), sizeof(#n)-1, (v), (l) TSRMLS_CC)
#	define SET_PROP(n, z) 					SET_PROP_EX(getThis(), n, z)
#	define SET_PROP_EX(this, n, z) 			zend_update_property(OBJ_PROP_CE, this, (#n), sizeof(#n)-1, (z) TSRMLS_CC)
#	define GET_PROP(n) 						GET_PROP_EX(getThis(), n)
#	define GET_PROP_EX(this, n) 			zend_read_property(OBJ_PROP_CE, this, (#n), sizeof(#n)-1, 0 TSRMLS_CC)

#	define DCL_CONST(t, n, v) zend_declare_class_constant_ ##t(OBJ_PROP_CE, (n), sizeof(n)-1, (v) TSRMLS_CC)

#	define ACC_PROP_PRIVATE(ce, flags)		((flags & ZEND_ACC_PRIVATE) && (EG(scope) && ce == EG(scope))
#	define ACC_PROP_PROTECTED(ce, flags)	((flags & ZEND_ACC_PROTECTED) && (zend_check_protected(ce, EG(scope))))
#	define ACC_PROP_PUBLIC(flags)			(flags & ZEND_ACC_PUBLIC)
#	define ACC_PROP(ce, flags)				(ACC_PROP_PUBLIC(flags) || ACC_PROP_PRIVATE(ce, flags) || ACC_PROP_PROTECTED(ce, flags))

#	define SET_EH_THROW() SET_EH_THROW_EX(zend_exception_get_default())
#	define SET_EH_THROW_HTTP() SET_EH_THROW_EX(http_exception_get_default())
#	define SET_EH_THROW_EX(ex) php_set_error_handling(EH_THROW, ex TSRMLS_CC)
#	define SET_EH_NORMAL() php_set_error_handling(EH_NORMAL, NULL TSRMLS_CC)

#endif /* ZEND_ENGINE_2 */
/* }}} */

#ifdef ZEND_ENGINE_2
#	define with_error_handling(eh, ec) \
	{ \
		error_handling_t __eh = PG(error_handling); \
		zend_class_entry *__ec= PG(exception_class); \
		php_set_error_handling(eh, ec TSRMLS_CC);
#	define end_error_handling() \
		php_set_error_handling(__eh, __ec TSRMLS_CC); \
	}
#else
#	define with_error_handling(eh, ec)
#	define end_error_handling()
#endif

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 2) || PHP_MAJOR_VERSION > 5
#	define	ZEND_EXCEPTION_GET_DEFAULT() zend_exception_get_default(TSRMLS_C)
#else
#	define	ZEND_EXCEPTION_GET_DEFAULT() zend_exception_get_default()
#endif

#ifndef E_THROW
#	define E_THROW 0
#endif
#ifdef ZEND_ENGINE_2
#	define HE_THROW		E_THROW TSRMLS_CC
#	define HE_NOTICE	(HTTP_G->only_exceptions ? E_THROW : E_NOTICE) TSRMLS_CC
#	define HE_WARNING	(HTTP_G->only_exceptions ? E_THROW : E_WARNING) TSRMLS_CC
#	define HE_ERROR		(HTTP_G->only_exceptions ? E_THROW : E_ERROR) TSRMLS_CC
#else
#	define HE_THROW		E_WARNING TSRMLS_CC
#	define HE_NOTICE	E_NOTICE TSRMLS_CC
#	define HE_WARNING	E_WARNING TSRMLS_CC
#	define HE_ERROR		E_ERROR TSRMLS_CC
#endif

#define HTTP_E_RUNTIME				1L
#define HTTP_E_INVALID_PARAM		2L
#define HTTP_E_HEADER				3L
#define HTTP_E_MALFORMED_HEADERS	4L
#define HTTP_E_REQUEST_METHOD		5L
#define HTTP_E_MESSAGE_TYPE			6L
#define HTTP_E_ENCODING				7L
#define HTTP_E_REQUEST				8L
#define HTTP_E_REQUEST_POOL			9L
#define HTTP_E_SOCKET				10L
#define HTTP_E_RESPONSE				11L
#define HTTP_E_URL					12L
#define HTTP_E_QUERYSTRING			13L

#ifdef ZEND_ENGINE_2
#	define HTTP_BEGIN_ARGS_EX(class, method, ret_ref, req_args)	static ZEND_BEGIN_ARG_INFO_EX(args_for_ ##class## _ ##method , 0, ret_ref, req_args)
#	define HTTP_BEGIN_ARGS_AR(class, method, ret_ref, req_args)	static ZEND_BEGIN_ARG_INFO_EX(args_for_ ##class## _ ##method , 1, ret_ref, req_args)
#	define HTTP_END_ARGS										}
#	define HTTP_EMPTY_ARGS_EX(class, method, ret_ref)			HTTP_BEGIN_ARGS_EX(class, method, ret_ref, 0) HTTP_END_ARGS
#	define HTTP_ARGS(class, method)								args_for_ ##class## _ ##method
#	define HTTP_ARG_VAL(name, pass_ref)							ZEND_ARG_INFO(pass_ref, name)
#	define HTTP_ARG_OBJ(class, name, allow_null)				ZEND_ARG_OBJ_INFO(0, name, class, allow_null)
#endif

#ifdef ZEND_ENGINE_2
#	define EMPTY_FUNCTION_ENTRY {NULL, NULL, NULL, 0, 0}
#else
#	define EMPTY_FUNCTION_ENTRY {NULL, NULL, NULL}
#endif

#ifdef HTTP_HAVE_CURL
#	ifdef ZEND_ENGINE_2
#		define HTTP_DECLARE_ARG_PASS_INFO() \
			static \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_2, 0) \
				ZEND_ARG_PASS_INFO(0) \
				ZEND_ARG_PASS_INFO(1) \
			ZEND_END_ARG_INFO(); \
 \
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
			ZEND_END_ARG_INFO();

#	else
#		define HTTP_DECLARE_ARG_PASS_INFO() \
			static unsigned char http_arg_pass_ref_2[] = {2, BYREF_NONE, BYREF_FORCE}; \
			static unsigned char http_arg_pass_ref_3[] = {3, BYREF_NONE, BYREF_NONE, BYREF_FORCE}; \
			static unsigned char http_arg_pass_ref_4[] = {4, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_FORCE}; \
			static unsigned char http_arg_pass_ref_5[] = {5, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_FORCE};
#	endif /* ZEND_ENGINE_2 */
#else
#	ifdef ZEND_ENGINE_2
#		define HTTP_DECLARE_ARG_PASS_INFO() \
			static \
			ZEND_BEGIN_ARG_INFO(http_arg_pass_ref_2, 0) \
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
			ZEND_END_ARG_INFO();
#	else
#		define HTTP_DECLARE_ARG_PASS_INFO() \
			static unsigned char http_arg_pass_ref_2[] = {2, BYREF_NONE, BYREF_FORCE}; \
			static unsigned char http_arg_pass_ref_4[] = {4, BYREF_NONE, BYREF_NONE, BYREF_NONE, BYREF_FORCE};
#	endif /* ZEND_ENGINE_2 */
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
#		define TSRMLS_SET_CTX(ctx)	ctx = (void ***) tsrm_ls
#	else
#		define TSRMLS_SET_CTX(ctx)
#	endif
#endif

#ifndef ZVAL_ZVAL
#define ZVAL_ZVAL(z, zv, copy, dtor) {  \
		int is_ref, refcount;           \
		is_ref = (z)->is_ref;           \
		refcount = (z)->refcount;       \
		*(z) = *(zv);                   \
		if (copy) {                     \
			zval_copy_ctor(z);          \
	    }                               \
		if (dtor) {                     \
			if (!copy) {                \
				ZVAL_NULL(zv);          \
			}                           \
			zval_ptr_dtor(&zv);         \
	    }                               \
		(z)->is_ref = is_ref;           \
		(z)->refcount = refcount;       \
	}
#endif
#ifndef RETVAL_ZVAL
#define RETVAL_ZVAL(zv, copy, dtor)		ZVAL_ZVAL(return_value, zv, copy, dtor)
#endif
#ifndef RETURN_ZVAL
#define RETURN_ZVAL(zv, copy, dtor)		{ RETVAL_ZVAL(zv, copy, dtor); return; }
#endif

#define PHP_MINIT_CALL(func) PHP_MINIT(func)(INIT_FUNC_ARGS_PASSTHRU)
#define PHP_RINIT_CALL(func) PHP_RINIT(func)(INIT_FUNC_ARGS_PASSTHRU)
#define PHP_MSHUTDOWN_CALL(func) PHP_MSHUTDOWN(func)(SHUTDOWN_FUNC_ARGS_PASSTHRU)
#define PHP_RSHUTDOWN_CALL(func) PHP_RSHUTDOWN(func)(SHUTDOWN_FUNC_ARGS_PASSTHRU)

#define Z_OBJ_DELREF(z) \
	if (Z_OBJ_HT(z)->del_ref) { \
		Z_OBJ_HT(z)->del_ref(&(z) TSRMLS_CC); \
	}
#define Z_OBJ_ADDREF(z) \
	if (Z_OBJ_HT(z)->add_ref) { \
		Z_OBJ_HT(z)->add_ref(&(z) TSRMLS_CC); \
	}
#define Z_OBJ_DELREF_P(z) \
	if (Z_OBJ_HT_P(z)->del_ref) { \
		Z_OBJ_HT_P(z)->del_ref((z) TSRMLS_CC); \
	}
#define Z_OBJ_ADDREF_P(z) \
	if (Z_OBJ_HT_P(z)->add_ref) { \
		Z_OBJ_HT_P(z)->add_ref((z) TSRMLS_CC); \
	}
#define Z_OBJ_DELREF_PP(z) \
	if (Z_OBJ_HT_PP(z)->del_ref) { \
		Z_OBJ_HT_PP(z)->del_ref(*(z) TSRMLS_CC); \
	}
#define Z_OBJ_ADDREF_PP(z) \
	if (Z_OBJ_HT_PP(z)->add_ref) { \
		Z_OBJ_HT_PP(z)->add_ref(*(z) TSRMLS_CC); \
	}

#endif /* PHP_HTTP_STD_DEFS_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

