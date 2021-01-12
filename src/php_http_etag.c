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

#include "php_http_api.h"

#include "ext/hash/php_hash.h"
#include "ext/standard/crc32.h"
#include "ext/standard/sha1.h"
#include "ext/standard/md5.h"

#if PHP_VERSION_ID >= 80100
# define HASH_INIT_ARGS ,NULL
#else
# define HASH_INIT_ARGS
#endif

php_http_etag_t *php_http_etag_init(const char *mode)
{
	php_http_etag_t *e;
	zend_string *mode_str = zend_string_init(mode, strlen(mode), 0);
	const php_hash_ops *eho = php_hash_fetch_ops(mode_str);

	if (!eho) {
		zend_string_release(mode_str);
		return NULL;
	}
	zend_string_release(mode_str);

	e = emalloc(sizeof(*e) + eho->context_size - 1);
	e->ops = eho;
	eho->hash_init(e->ctx HASH_INIT_ARGS);

	return e;
}

char *php_http_etag_finish(php_http_etag_t *e)
{
	unsigned char digest[128] = {0};
	char *etag;

	e->ops->hash_final(digest, e->ctx);
	etag = php_http_etag_digest(digest, e->ops->digest_size);
	efree(e);

	return etag;
}

size_t php_http_etag_update(php_http_etag_t *e, const char *data_ptr, size_t data_len)
{
	e->ops->hash_update(e->ctx, (const unsigned char *) data_ptr, data_len);

	return data_len;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
