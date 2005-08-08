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

#ifndef PHP_HTTP_MISSING
#define PHP_HTTP_MISSING

#include "php_version.h"

#ifdef ZEND_ENGINE_2

#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION == 0)
extern int zend_declare_property_double(zend_class_entry *ce, char *name, int name_length, double value, int access_type TSRMLS_DC);
extern void zend_update_property_double(zend_class_entry *scope, zval *object, char *name, int name_length, double value TSRMLS_DC);

extern int zend_declare_property_bool(zend_class_entry *ce, char *name, int name_length, long value, int access_type TSRMLS_DC);
extern void zend_update_property_bool(zend_class_entry *scope, zval *object, char *name, int name_length, long value TSRMLS_DC);
#endif

#if (PHP_MAJOR_VERSION >= 5)
extern int zend_declare_class_constant(zend_class_entry *ce, char *name, size_t name_length, zval *value TSRMLS_DC);
extern int zend_declare_class_constant_null(zend_class_entry *ce, char *name, size_t name_length TSRMLS_DC);
extern int zend_declare_class_constant_long(zend_class_entry *ce, char *name, size_t name_length, long value TSRMLS_DC);
extern int zend_declare_class_constant_bool(zend_class_entry *ce, char *name, size_t name_length, zend_bool value TSRMLS_DC);
extern int zend_declare_class_constant_double(zend_class_entry *ce, char *name, size_t name_length, double value TSRMLS_DC);
extern int zend_declare_class_constant_string(zend_class_entry *ce, char *name, size_t name_length, char *value TSRMLS_DC);
extern int zend_declare_class_constant_stringl(zend_class_entry *ce, char *name, size_t name_length, char *value, size_t value_length TSRMLS_DC);
#endif

extern int zend_update_static_property(zend_class_entry *scope, char *name, size_t name_len, zval *value TSRMLS_DC);
extern int zend_update_static_property_bool(zend_class_entry *scope, char *name, size_t name_len, zend_bool value TSRMLS_DC);
extern int zend_update_static_property_long(zend_class_entry *scope, char *name, size_t name_len, long value TSRMLS_DC);
extern int zend_update_static_property_double(zend_class_entry *scope, char *name, size_t name_len, double value TSRMLS_DC);
extern int zend_update_static_property_string(zend_class_entry *scope, char *name, size_t name_len, char *value TSRMLS_DC);
extern int zend_update_static_property_stringl(zend_class_entry *scope, char *name, size_t name_len, char *value, size_t value_len TSRMLS_DC);

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

