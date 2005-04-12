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

#ifndef PHP_HTTP_EXCEPTION_OBJECT_H
#define PHP_HTTP_EXCEPTION_OBJECT_H
#ifdef ZEND_ENGINE_2

extern zend_class_entry *http_exception_object_ce;
extern zend_function_entry http_exception_object_fe[];

#define http_exception_object_init _http_exception_object_init
extern void _http_exception_object_init(INIT_FUNC_ARGS);

#define http_exception_get_default _http_exception_get_default
extern zend_class_entry *_http_exception_get_default();

#define http_exception_throw() http_exception_throw_ex(0)
#define http_exception_throw_ex(code) http_exception_throw_ce_ex(NULL, code)
#define http_exception_throw_ce(ce) http_exception_throw_ce_ex(ce, 0)
#define http_exception_throw_ce_ex(ce, code) _http_exception_throw_ce_ex(ce, code TSRMLS_CC)
extern void _http_exception_throw_ce_ex(zend_class_entry *ce, int code TSRMLS_DC);

#define HTTP_E_UNKOWN 0

#endif
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

