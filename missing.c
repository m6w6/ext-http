
#include "php.h"
#include "missing.h"

int zend_declare_property_double(zend_class_entry *ce, char *name, int name_length, double value, int access_type TSRMLS_DC)
{
	zval *property;

	if (ce->type & ZEND_INTERNAL_CLASS) {
		property = malloc(sizeof(zval));
	} else {
		ALLOC_ZVAL(property);
	}
	INIT_PZVAL(property);
	ZVAL_DOUBLE(property, value);
	return zend_declare_property(ce, name, name_length, property, access_type TSRMLS_CC);
}

void zend_update_property_double(zend_class_entry *scope, zval *object, char *name, int name_length, double value TSRMLS_DC)
{
	zval *tmp;

	ALLOC_ZVAL(tmp);
	tmp->is_ref = 0;
	tmp->refcount = 0;
	ZVAL_DOUBLE(tmp, value);
	zend_update_property(scope, object, name, name_length, tmp TSRMLS_CC);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

