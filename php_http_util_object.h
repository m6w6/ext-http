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

#ifndef PHP_HTTP_UTIL_OBJECT_H
#define PHP_HTTP_UTIL_OBJECT_H
#ifdef ZEND_ENGINE_2

extern zend_class_entry *http_util_object_ce;
extern zend_function_entry http_util_object_fe[];

#define http_util_object_init() _http_util_object_init(INIT_FUNC_ARGS_PASSTHRU)
extern void _http_util_object_init(INIT_FUNC_ARGS);

PHP_METHOD(HttpUtil, date);
PHP_METHOD(HttpUtil, absoluteUri);
PHP_METHOD(HttpUtil, negotiateLanguage);
PHP_METHOD(HttpUtil, negotiateCharset);
PHP_METHOD(HttpUtil, matchModified);
PHP_METHOD(HttpUtil, matchEtag);
PHP_METHOD(HttpUtil, chunkedDecode);
PHP_METHOD(HttpUtil, parseHeaders);
PHP_METHOD(HttpUtil, parseMessage);

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

