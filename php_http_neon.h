#ifndef PHP_HTTP_NEON_H
#define PHP_HTTP_NEON_H

php_http_request_ops_t *php_http_neon_get_request_ops(void);

PHP_MINIT_FUNCTION(http_neon);
PHP_MSHUTDOWN_FUNCTION(http_neon);

extern zend_class_entry *php_http_neon_class_entry;
extern zend_function_entry php_http_neon_method_entry[];

#define php_http_neon_new php_http_object_new

PHP_METHOD(HttpNEON, __construct);

#endif /* PHP_HTTP_NEON_H */
