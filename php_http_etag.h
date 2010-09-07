#ifndef PHP_HTTP_ETAG_H
#define PHP_HTTP_ETAG_H

PHP_HTTP_API void *php_http_etag_init(TSRMLS_D);
PHP_HTTP_API size_t php_http_etag_update(void *ctx, const char *data_ptr, size_t data_len TSRMLS_DC);
PHP_HTTP_API char *php_http_etag_finish(void *ctx TSRMLS_DC);

static inline char *php_http_etag_digest(const unsigned char *digest, int len)
{
	static const char hexdigits[17] = "0123456789abcdef";
	int i;
	char *hex = emalloc(len * 2 + 1);
	char *ptr = hex;

	for (i = 0; i < len; ++i) {
		*ptr++ = hexdigits[digest[i] >> 4];
		*ptr++ = hexdigits[digest[i] & 0xF];
	}
	*ptr = '\0';

	return hex;
}

#endif /* PHP_HTTP_ETAG_H */
