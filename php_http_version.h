
#ifndef PHP_HTTP_VERSION_H
#define	PHP_HTTP_VERSION_H

typedef struct php_http_version {
	unsigned major;
	unsigned minor;
} php_http_version_t;

PHP_HTTP_API php_http_version_t *php_http_version_init(php_http_version_t *v, unsigned major, unsigned minor TSRMLS_DC);
PHP_HTTP_API php_http_version_t *php_http_version_parse(php_http_version_t *v, const char *str TSRMLS_DC);
PHP_HTTP_API void php_http_version_to_string(php_http_version_t *v, char **str, size_t *len, const char *pre, const char *post TSRMLS_DC);
PHP_HTTP_API void php_http_version_to_struct(php_http_version_t *v, HashTable *strct TSRMLS_DC);
PHP_HTTP_API void php_http_version_dtor(php_http_version_t *v);
PHP_HTTP_API void php_http_version_free(php_http_version_t **v);

#endif	/* PHP_HTTP_VERSION_H */

