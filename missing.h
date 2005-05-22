
#ifndef PHP_HTTP_MISSING
#define PHP_HTTP_MISSING

extern int zend_declare_property_double(zend_class_entry *ce, char *name, int name_length, double value, int access_type TSRMLS_DC);
extern void zend_update_property_double(zend_class_entry *scope, zval *object, char *name, int name_length, double value TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

