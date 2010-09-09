/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2010, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id: http_exception_object.c 292841 2009-12-31 08:48:57Z mike $ */

#include "php_http.h"

#include <Zend/zend_exceptions.h>

#ifndef PHP_HTTP_DBG_EXCEPTIONS
#	define PHP_HTTP_DBG_EXCEPTIONS 0
#endif

zend_class_entry *PHP_HTTP_EX_DEF_CE;
zend_class_entry *PHP_HTTP_EX_CE(runtime);
zend_class_entry *PHP_HTTP_EX_CE(header);
zend_class_entry *PHP_HTTP_EX_CE(malformed_headers);
zend_class_entry *PHP_HTTP_EX_CE(request_method);
zend_class_entry *PHP_HTTP_EX_CE(message);
zend_class_entry *PHP_HTTP_EX_CE(message_type);
zend_class_entry *PHP_HTTP_EX_CE(invalid_param);
zend_class_entry *PHP_HTTP_EX_CE(encoding);
zend_class_entry *PHP_HTTP_EX_CE(request);
zend_class_entry *PHP_HTTP_EX_CE(request_pool);
zend_class_entry *PHP_HTTP_EX_CE(socket);
zend_class_entry *PHP_HTTP_EX_CE(response);
zend_class_entry *PHP_HTTP_EX_CE(url);
zend_class_entry *PHP_HTTP_EX_CE(querystring);
zend_class_entry *PHP_HTTP_EX_CE(cookie);

zend_function_entry php_http_exception_method_entry[] = {
	EMPTY_FUNCTION_ENTRY
};

#if PHP_HTTP_DBG_EXCEPTIONS
static void php_http_exception_hook(zval *ex TSRMLS_DC)
{
	if (ex) {
		zval *m = zend_read_property(Z_OBJCE_P(ex), ex, "message", lenof("message"), 0 TSRMLS_CC);
		fprintf(stderr, "*** Threw exception '%s'\n", Z_STRVAL_P(m));
	} else {
		fprintf(stderr, "*** Threw NULL exception\n");
	}
}
#endif

PHP_MINIT_FUNCTION(http_exception)
{
	PHP_HTTP_REGISTER_EXCEPTION(Exception, PHP_HTTP_EX_DEF_CE, zend_exception_get_default(TSRMLS_C));
	
	PHP_HTTP_REGISTER_EXCEPTION(RuntimeException, PHP_HTTP_EX_CE(runtime), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(InvalidParamException, PHP_HTTP_EX_CE(invalid_param), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(HeaderException, PHP_HTTP_EX_CE(header), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(MalformedHeadersException, PHP_HTTP_EX_CE(malformed_headers), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(RequestMethodException, PHP_HTTP_EX_CE(request_method), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(MessageException, PHP_HTTP_EX_CE(message), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(MessageTypeException, PHP_HTTP_EX_CE(message_type), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(EncodingException, PHP_HTTP_EX_CE(encoding), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(RequestException, PHP_HTTP_EX_CE(request), PHP_HTTP_EX_DEF_CE);

	zend_declare_property_long(PHP_HTTP_EX_CE(request), "curlCode", lenof("curlCode"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);

	PHP_HTTP_REGISTER_EXCEPTION(RequestPoolException, PHP_HTTP_EX_CE(request_pool), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(SocketException, PHP_HTTP_EX_CE(socket), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(ResponseException, PHP_HTTP_EX_CE(response), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(UrlException, PHP_HTTP_EX_CE(url), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(QueryStringException, PHP_HTTP_EX_CE(querystring), PHP_HTTP_EX_DEF_CE);
	PHP_HTTP_REGISTER_EXCEPTION(CookieException, PHP_HTTP_EX_CE(cookie), PHP_HTTP_EX_DEF_CE);
	
#if PHP_HTTP_DBG_EXCEPTIONS
	zend_throw_exception_hook = php_http_exception_hook;
#endif
	
	return SUCCESS;
}

zend_class_entry *php_http_exception_get_default(void)
{
	return PHP_HTTP_EX_DEF_CE;
}

zend_class_entry *php_http_exception_get_for_code(long code)
{
	zend_class_entry *ex = PHP_HTTP_EX_DEF_CE;

	switch (code) {
		case PHP_HTTP_E_RUNTIME:					ex = PHP_HTTP_EX_CE(runtime);					break;
		case PHP_HTTP_E_INVALID_PARAM:				ex = PHP_HTTP_EX_CE(invalid_param);				break;
		case PHP_HTTP_E_HEADER:						ex = PHP_HTTP_EX_CE(header);					break;
		case PHP_HTTP_E_MALFORMED_HEADERS:			ex = PHP_HTTP_EX_CE(malformed_headers);			break;
		case PHP_HTTP_E_REQUEST_METHOD:				ex = PHP_HTTP_EX_CE(request_method);			break;
		case PHP_HTTP_E_MESSAGE:					ex = PHP_HTTP_EX_CE(message);					break;
		case PHP_HTTP_E_MESSAGE_TYPE:				ex = PHP_HTTP_EX_CE(message_type);				break;
		case PHP_HTTP_E_ENCODING:					ex = PHP_HTTP_EX_CE(encoding);					break;
		case PHP_HTTP_E_REQUEST:					ex = PHP_HTTP_EX_CE(request);					break;
		case PHP_HTTP_E_REQUEST_POOL:				ex = PHP_HTTP_EX_CE(request_pool);				break;
		case PHP_HTTP_E_SOCKET:						ex = PHP_HTTP_EX_CE(socket);					break;
		case PHP_HTTP_E_RESPONSE:					ex = PHP_HTTP_EX_CE(response);					break;
		case PHP_HTTP_E_URL:						ex = PHP_HTTP_EX_CE(url);						break;
		case PHP_HTTP_E_QUERYSTRING:				ex = PHP_HTTP_EX_CE(querystring);				break;
		case PHP_HTTP_E_COOKIE:						ex = PHP_HTTP_EX_CE(cookie);					break;
	}

	return ex;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

