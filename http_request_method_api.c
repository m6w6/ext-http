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

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_request_method_api.h"

#include "phpstr/phpstr.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

/* {{{ char *http_request_methods[] */
static const char *const http_request_methods[] = {
	"UNKOWN",
	/* HTTP/1.1 */
	"GET",
	"HEAD",
	"POST",
	"PUT",
	"DELETE",
	"OPTIONS",
	"TRACE",
	"CONNECT",
	/* WebDAV - RFC 2518 */
	"PROPFIND",
	"PROPPATCH",
	"MKCOL",
	"COPY",
	"MOVE",
	"LOCK",
	"UNLOCK",
	/* WebDAV Versioning - RFC 3253 */
	"VERSION-CONTROL",
	"REPORT",
	"CHECKOUT",
	"CHECKIN",
	"UNCHECKOUT",
	"MKWORKSPACE",
	"UPDATE",
	"LABEL",
	"MERGE",
	"BASELINE-CONTROL",
	"MKACTIVITY",
	/* WebDAV Access Control - RFC 3744 */
	"ACL",
	NULL
};
/* }}} */

/* {{{ char *http_request_method_name(http_request_method) */
PHP_HTTP_API const char *_http_request_method_name(http_request_method m TSRMLS_DC)
{
	zval **meth;

	if (HTTP_STD_REQUEST_METHOD(m)) {
		return http_request_methods[m];
	}

	if (SUCCESS == zend_hash_index_find(&HTTP_G(request).methods.custom, HTTP_CUSTOM_REQUEST_METHOD(m), (void **) &meth)) {
		return Z_STRVAL_PP(meth);
	}

	return http_request_methods[0];
}
/* }}} */

/* {{{ unsigned long http_request_method_exists(zend_bool, unsigned long, char *) */
PHP_HTTP_API unsigned long _http_request_method_exists(zend_bool by_name, unsigned long id, const char *name TSRMLS_DC)
{
	if (by_name) {
		unsigned i;

		for (i = HTTP_NO_REQUEST_METHOD + 1; i < HTTP_MAX_REQUEST_METHOD; ++i) {
			if (!strcmp(name, http_request_methods[i])) {
				return i;
			}
		}
		{
			zval **data;
			char *key;
			ulong idx;

			FOREACH_HASH_KEYVAL(&HTTP_G(request).methods.custom, key, idx, data) {
				if (!strcmp(name, Z_STRVAL_PP(data))) {
					return idx + HTTP_MAX_REQUEST_METHOD;
				}
			}
		}
		return 0;
	} else {
		return HTTP_STD_REQUEST_METHOD(id) || zend_hash_index_exists(&HTTP_G(request).methods.custom, HTTP_CUSTOM_REQUEST_METHOD(id)) ? id : 0;
	}
}
/* }}} */

/* {{{ unsigned long http_request_method_register(char *) */
PHP_HTTP_API unsigned long _http_request_method_register(const char *method TSRMLS_DC)
{
	zval array;
	char *http_method;
	unsigned long meth_num = HTTP_G(request).methods.custom.nNextFreeElement + HTTP_MAX_REQUEST_METHOD;

	Z_ARRVAL(array) = &HTTP_G(request).methods.custom;
	add_next_index_string(&array, estrdup(method), 0);

	spprintf(&http_method, 0, "HTTP_%s", method);
	zend_register_long_constant(http_method, strlen(http_method) + 1, meth_num, CONST_CS, http_module_number TSRMLS_CC);
	efree(http_method);

	return meth_num;
}
/* }}} */

/* {{{ STATUS http_request_method_unregister(usngigned long) */
PHP_HTTP_API STATUS _http_request_method_unregister(unsigned long method TSRMLS_DC)
{
	zval **zmethod;
	char *http_method;

	if (SUCCESS != zend_hash_index_find(&HTTP_G(request).methods.custom, HTTP_CUSTOM_REQUEST_METHOD(method), (void **) &zmethod)) {
		http_error_ex(HE_NOTICE, HTTP_E_REQUEST_METHOD, "Request method with id %lu does not exist", method);
		return FAILURE;
	}

	spprintf(&http_method, 0, "HTTP_%s", Z_STRVAL_PP(zmethod));

	if (	(SUCCESS != zend_hash_index_del(&HTTP_G(request).methods.custom, HTTP_CUSTOM_REQUEST_METHOD(method)))
		||	(SUCCESS != zend_hash_del(EG(zend_constants), http_method, strlen(http_method) + 1))) {
		http_error_ex(HE_NOTICE, HTTP_E_REQUEST_METHOD, "Could not unregister request method: %s", http_method);
		efree(http_method);
		return FAILURE;
	}

	efree(http_method);
	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

