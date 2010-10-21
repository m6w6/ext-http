#ifndef PHP_HTTP_NEON_H
#define PHP_HTTP_NEON_H

php_http_request_ops_t *php_http_neon_get_request_ops(void);

PHP_MINIT_FUNCTION(http_neon);
PHP_MSHUTDOWN_FUNCTION(http_neon);

#endif /* PHP_HTTP_NEON_H */
