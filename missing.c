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

#include "php.h"
#include "missing.h"

#if PHP_MAJOR_VERSION == 5

static inline zval *new_zval(int persistent)
{
	zval *z = pemalloc(sizeof(zval), persistent);
	INIT_PZVAL(z);
	return z;
}

static inline zval *tmp_zval(void)
{
	zval *z;
	ALLOC_ZVAL(z);
	z->is_ref = 0;
	z->refcount = 0;
	return z;
}

static void dup_zval(zval **z)
{
	zval *o = *z;
	zval_add_ref(z);
	SEPARATE_ZVAL(z);
}


#	if PHP_MINOR_VERSION == 0

int zend_declare_property_double(zend_class_entry *ce, char *name, int name_length, double value, int access_type TSRMLS_DC)
{
	zval *property = new_zval(ce->type & ZEND_INTERNAL_CLASS);
	ZVAL_DOUBLE(property, value);
	return zend_declare_property(ce, name, name_length, property, access_type TSRMLS_CC);
}

void zend_update_property_double(zend_class_entry *scope, zval *object, char *name, int name_length, double value TSRMLS_DC)
{
	zval *tmp = tmp_zval();
	ZVAL_DOUBLE(tmp, value);
	zend_update_property(scope, object, name, name_length, tmp TSRMLS_CC);
}

int zend_declare_property_bool(zend_class_entry *ce, char *name, int name_length, long value, int access_type TSRMLS_DC)
{
	zval *property = new_zval(ce->type & ZEND_INTERNAL_CLASS);
	ZVAL_BOOL(property, value);
	return zend_declare_property(ce, name, name_length, property, access_type TSRMLS_CC);
}

void zend_update_property_bool(zend_class_entry *scope, zval *object, char *name, int name_length, long value TSRMLS_DC)
{
	zval *tmp = tmp_zval();
	ZVAL_BOOL(tmp, value);
	zend_update_property(scope, object, name, name_length, tmp TSRMLS_CC);
}

#	endif /* PHP_MINOR_VERSION == 0 */

int zend_declare_class_constant(zend_class_entry *ce, char *name, size_t name_length, zval *value TSRMLS_DC)
{
	return zend_hash_add(&ce->constants_table, name, name_length, &value, sizeof(zval *), NULL);
}

int zend_declare_class_constant_long(zend_class_entry *ce, char *name, size_t name_length, long value TSRMLS_DC)
{
	zval *constant = new_zval(ce->type & ZEND_INTERNAL_CLASS);
	ZVAL_LONG(constant, value);
	return zend_declare_class_constant(ce, name, name_length, constant TSRMLS_CC);
}

int zend_declare_class_constant_bool(zend_class_entry *ce, char *name, size_t name_length, zend_bool value TSRMLS_DC)
{
	zval *constant = new_zval(ce->type & ZEND_INTERNAL_CLASS);
	ZVAL_BOOL(constant, value);
	return zend_declare_class_constant(ce, name, name_length, constant TSRMLS_CC);
}

int zend_declare_class_constant_double(zend_class_entry *ce, char *name, size_t name_length, double value TSRMLS_DC)
{
	zval *constant = new_zval(ce->type & ZEND_INTERNAL_CLASS);
	ZVAL_DOUBLE(constant, value);
	return zend_declare_class_constant(ce, name, name_length, constant TSRMLS_CC);
}

int zend_declare_class_constant_string(zend_class_entry *ce, char *name, size_t name_length, char *value TSRMLS_DC)
{
	return zend_declare_class_constant_stringl(ce, name, name_length, value, strlen(value) TSRMLS_CC);
}

int zend_declare_class_constant_stringl(zend_class_entry *ce, char *name, size_t name_length, char *value, size_t value_length TSRMLS_DC)
{
	zval *constant = new_zval(ce->type & ZEND_INTERNAL_CLASS);
	if (ce->type & ZEND_INTERNAL_CLASS) {
		ZVAL_STRINGL(constant, zend_strndup(value, value_length), value_length, 0);
	} else {
		ZVAL_STRINGL(constant, value, value_length, 1);
	}
	return zend_declare_class_constant(ce, name, name_length, constant TSRMLS_CC);
}

int zend_update_static_property(zend_class_entry *scope, char *name, size_t name_len, zval *value TSRMLS_DC)
{
	int retval;
	zval **property = NULL;
	zend_class_entry *old_scope = EG(scope);
	
	EG(scope) = scope;
	
	if (!(property = zend_std_get_static_property(scope, name, name_len, 0 TSRMLS_CC))) {
		retval = FAILURE;
	} else if (*property == value) {
		retval = SUCCESS;
	} else {
		value->refcount++;
		if (PZVAL_IS_REF(*property)) {
			zval_dtor(*property);
			(*property)->type = value->type;
			(*property)->value = value->value;
			
			if (value->refcount) {
				zval_copy_ctor(*property);
			}
		} else {
			**property = *value;
			zval_copy_ctor(*property);
		}
		retval = SUCCESS;
	}
	zval_ptr_dtor(&value);
	EG(scope) = old_scope;
	
	return retval;
}

int zend_update_static_property_bool(zend_class_entry *scope, char *name, size_t name_len, zend_bool value TSRMLS_DC)
{
	zval *tmp = tmp_zval();
	ZVAL_BOOL(tmp, value);
	return zend_update_static_property(scope, name, name_len, tmp TSRMLS_CC);
}

int zend_update_static_property_long(zend_class_entry *scope, char *name, size_t name_len, long value TSRMLS_DC)
{
	zval *tmp = tmp_zval();
	ZVAL_LONG(tmp, value);
	return zend_update_static_property(scope, name, name_len, tmp TSRMLS_CC);
}

int zend_update_static_property_double(zend_class_entry *scope, char *name, size_t name_len, double value TSRMLS_DC)
{
	zval *tmp = tmp_zval();
	ZVAL_DOUBLE(tmp, value);
	return zend_update_static_property(scope, name, name_len, tmp TSRMLS_CC);
}

int zend_update_static_property_string(zend_class_entry *scope, char *name, size_t name_len, char *value TSRMLS_DC)
{
	zval *tmp = tmp_zval();
	ZVAL_STRING(tmp, value, 1);
	return zend_update_static_property(scope, name, name_len, tmp TSRMLS_CC);
}

int zend_update_static_property_stringl(zend_class_entry *scope, char *name, size_t name_len, char *value, size_t value_len TSRMLS_DC)
{
	zval *tmp = tmp_zval();
	ZVAL_STRINGL(tmp, value, value_len, 1);
	return zend_update_static_property(scope, name, name_len, tmp TSRMLS_CC);
}

void zend_fix_static_properties(zend_class_entry *ce, HashTable *static_members TSRMLS_DC)
{
	zend_hash_copy(static_members, ce->static_members, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	zend_hash_destroy(ce->static_members);
	zend_hash_init_ex(ce->static_members, static_members->nNumOfElements, NULL, ZVAL_PTR_DTOR, 1, 0);
}

void zend_init_static_properties(zend_class_entry *ce, HashTable *static_members TSRMLS_DC)
{
	zend_hash_copy(ce->static_members, static_members, (copy_ctor_func_t) dup_zval, NULL, sizeof(zval *));
}

void zend_clean_static_properties(zend_class_entry *ce TSRMLS_DC)
{
	zend_hash_clean(ce->static_members);
}

#endif /* PHP_MAJOR_VERSION == 5 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

