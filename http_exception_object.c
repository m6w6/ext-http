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

#include "php_http.h"

#ifdef ZEND_ENGINE_2

#ifndef HTTP_DBG_EXCEPTIONS
#	define HTTP_DBG_EXCEPTIONS 0
#endif

#include "zend_interfaces.h"
#include "zend_exceptions.h"
#include "php_http_exception_object.h"

zend_class_entry *http_exception_object_ce;
zend_class_entry *HTTP_EX_CE(runtime);
zend_class_entry *HTTP_EX_CE(header);
zend_class_entry *HTTP_EX_CE(malformed_headers);
zend_class_entry *HTTP_EX_CE(request_method);
zend_class_entry *HTTP_EX_CE(message_type);
zend_class_entry *HTTP_EX_CE(invalid_param);
zend_class_entry *HTTP_EX_CE(encoding);
zend_class_entry *HTTP_EX_CE(request);
zend_class_entry *HTTP_EX_CE(request_pool);
zend_class_entry *HTTP_EX_CE(socket);
zend_class_entry *HTTP_EX_CE(response);
zend_class_entry *HTTP_EX_CE(url);
zend_class_entry *HTTP_EX_CE(querystring);

#define HTTP_EMPTY_ARGS(method)					HTTP_EMPTY_ARGS_EX(HttpException, method, 0)
#define HTTP_EXCEPTION_ME(method, visibility)	PHP_ME(HttpException, method, HTTP_ARGS(HttpException, method), visibility)

HTTP_EMPTY_ARGS(__toString);

zend_function_entry http_exception_object_fe[] = {
	HTTP_EXCEPTION_ME(__toString, ZEND_ACC_PUBLIC)
	
	EMPTY_FUNCTION_ENTRY
};

#if HTTP_DBG_EXCEPTIONS
static void http_exception_hook(zval *ex TSRMLS_DC)
{
	if (ex) {
		zval *m = zend_read_property(Z_OBJCE_P(ex), ex, "message", lenof("message"), 0 TSRMLS_CC);
		fprintf(stderr, "*** Threw exception '%s'\n", Z_STRVAL_P(m));
	} else {
		fprintf(stderr, "*** Threw NULL exception\n");
	}
}
#endif

PHP_MINIT_FUNCTION(http_exception_object)
{
	HTTP_REGISTER_CLASS(HttpException, http_exception_object, ZEND_EXCEPTION_GET_DEFAULT(), 0);
	
	zend_declare_property_null(HTTP_EX_DEF_CE, "innerException", lenof("innerException"), ZEND_ACC_PUBLIC TSRMLS_CC);
	
	HTTP_REGISTER_EXCEPTION(HttpRuntimeException, HTTP_EX_CE(runtime), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpInvalidParamException, HTTP_EX_CE(invalid_param), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpHeaderException, HTTP_EX_CE(header), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpMalformedHeadersException, HTTP_EX_CE(malformed_headers), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpRequestMethodException, HTTP_EX_CE(request_method), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpMessageTypeException, HTTP_EX_CE(message_type), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpEncodingException, HTTP_EX_CE(encoding), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpRequestException, HTTP_EX_CE(request), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpRequestPoolException, HTTP_EX_CE(request_pool), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpSocketException, HTTP_EX_CE(socket), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpResponseException, HTTP_EX_CE(response), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpUrlException, HTTP_EX_CE(url), HTTP_EX_DEF_CE);
	HTTP_REGISTER_EXCEPTION(HttpQueryStringException, HTTP_EX_CE(querystring), HTTP_EX_DEF_CE);
	
	HTTP_LONG_CONSTANT("HTTP_E_RUNTIME", HTTP_E_RUNTIME);
	HTTP_LONG_CONSTANT("HTTP_E_INVALID_PARAM", HTTP_E_INVALID_PARAM);
	HTTP_LONG_CONSTANT("HTTP_E_HEADER", HTTP_E_HEADER);
	HTTP_LONG_CONSTANT("HTTP_E_MALFORMED_HEADERS", HTTP_E_MALFORMED_HEADERS);
	HTTP_LONG_CONSTANT("HTTP_E_REQUEST_METHOD", HTTP_E_REQUEST_METHOD);
	HTTP_LONG_CONSTANT("HTTP_E_MESSAGE_TYPE", HTTP_E_MESSAGE_TYPE);
	HTTP_LONG_CONSTANT("HTTP_E_ENCODING", HTTP_E_ENCODING);
	HTTP_LONG_CONSTANT("HTTP_E_REQUEST", HTTP_E_REQUEST);
	HTTP_LONG_CONSTANT("HTTP_E_REQUEST_POOL", HTTP_E_REQUEST_POOL);
	HTTP_LONG_CONSTANT("HTTP_E_SOCKET", HTTP_E_SOCKET);
	HTTP_LONG_CONSTANT("HTTP_E_RESPONSE", HTTP_E_RESPONSE);
	HTTP_LONG_CONSTANT("HTTP_E_URL", HTTP_E_URL);
	HTTP_LONG_CONSTANT("HTTP_E_QUERYSTRING", HTTP_E_QUERYSTRING);
	
#if HTTP_DBG_EXCEPTIONS
	zend_throw_exception_hook=http_exception_hook;
#endif
	
	return SUCCESS;
}

zend_class_entry *_http_exception_get_default()
{
	return http_exception_object_ce;
}

zend_class_entry *_http_exception_get_for_code(long code)
{
	zend_class_entry *ex = http_exception_object_ce;

	switch (code) {
		case HTTP_E_RUNTIME:					ex = HTTP_EX_CE(runtime);					break;
		case HTTP_E_INVALID_PARAM:				ex = HTTP_EX_CE(invalid_param);				break;
		case HTTP_E_HEADER:						ex = HTTP_EX_CE(header);					break;
		case HTTP_E_MALFORMED_HEADERS:			ex = HTTP_EX_CE(malformed_headers);			break;
		case HTTP_E_REQUEST_METHOD:				ex = HTTP_EX_CE(request_method);			break;
		case HTTP_E_MESSAGE_TYPE:				ex = HTTP_EX_CE(message_type);				break;
		case HTTP_E_ENCODING:					ex = HTTP_EX_CE(encoding);					break;
		case HTTP_E_REQUEST:					ex = HTTP_EX_CE(request);					break;
		case HTTP_E_REQUEST_POOL:				ex = HTTP_EX_CE(request_pool);				break;
		case HTTP_E_SOCKET:						ex = HTTP_EX_CE(socket);					break;
		case HTTP_E_RESPONSE:					ex = HTTP_EX_CE(response);					break;
		case HTTP_E_URL:						ex = HTTP_EX_CE(url);						break;
		case HTTP_E_QUERYSTRING:				ex = HTTP_EX_CE(querystring);				break;
	}

	return ex;
}

PHP_METHOD(HttpException, __toString)
{
	phpstr full_str;
	zend_class_entry *ce;
	zval *zobj = getThis(), *retval = NULL, *m, *f, *l;
	
	phpstr_init(&full_str);
	
	do {
		ce = Z_OBJCE_P(zobj);
		
		m = zend_read_property(ce, zobj, "message", lenof("message"), 0 TSRMLS_CC);
		f = zend_read_property(ce, zobj, "file", lenof("file"), 0 TSRMLS_CC);
		l = zend_read_property(ce, zobj, "line", lenof("line"), 0 TSRMLS_CC);
		
		if (m && f && l && Z_TYPE_P(m) == IS_STRING && Z_TYPE_P(f) == IS_STRING && Z_TYPE_P(l) == IS_LONG) {
			if (zobj != getThis()) {
				phpstr_appends(&full_str, " inner ");
			}
		
			phpstr_appendf(&full_str, "exception '%.*s' with message '%.*s' in %.*s:%ld" PHP_EOL,
				ce->name_length, ce->name, Z_STRLEN_P(m), Z_STRVAL_P(m), Z_STRLEN_P(f), Z_STRVAL_P(f), Z_LVAL_P(l)
			);
		} else {
			break;
		}
		
		zobj = zend_read_property(ce, zobj, "innerException", lenof("innerException"), 0 TSRMLS_CC);
	} while (Z_TYPE_P(zobj) == IS_OBJECT);
	
	if (zend_call_method_with_0_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "gettraceasstring", &retval) && Z_TYPE_P(retval) == IS_STRING) {
		phpstr_appends(&full_str, "Stack trace:" PHP_EOL);
		phpstr_append(&full_str, Z_STRVAL_P(retval), Z_STRLEN_P(retval));
		zval_ptr_dtor(&retval);
	}
	
	RETURN_PHPSTR_VAL(&full_str);
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

