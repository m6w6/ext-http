/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_ETAG_H
#define PHP_HTTP_ETAG_H

typedef struct php_http_etag {
	void *ctx;
	char *mode;
} php_http_etag_t;

PHP_HTTP_API php_http_etag_t *php_http_etag_init(const char *mode);
PHP_HTTP_API size_t php_http_etag_update(php_http_etag_t *e, const char *data_ptr, size_t data_len);
PHP_HTTP_API char *php_http_etag_finish(php_http_etag_t *e);

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

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

