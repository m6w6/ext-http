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

#if PHP_HTTP_HAVE_HASH
#	include "ext/hash/php_hash.h"
#endif

#include "ext/standard/crc32.h"
#include "ext/standard/sha1.h"
#include "ext/standard/md5.h"

php_http_etag_t *php_http_etag_init(const char *mode)
{
	void *ctx;
	php_http_etag_t *e;

	if (mode && (!strcasecmp(mode, "crc32b"))) {
		ctx = emalloc(sizeof(uint));
		*((uint *) ctx) = ~0;
	} else if (mode && !strcasecmp(mode, "sha1")) {
		PHP_SHA1Init(ctx = emalloc(sizeof(PHP_SHA1_CTX)));
	} else if (mode && !strcasecmp(mode, "md5")) {
		PHP_MD5Init(ctx = emalloc(sizeof(PHP_MD5_CTX)));
	} else {
#if PHP_HTTP_HAVE_HASH
		const php_hash_ops *eho = NULL;

		if (mode && (eho = php_hash_fetch_ops(mode, strlen(mode)))) {
			ctx = emalloc(eho->context_size);
			eho->hash_init(ctx);
		} else
#endif
		return NULL;
	}

	e = emalloc(sizeof(*e));
	e->ctx = ctx;
	e->mode = estrdup(mode);

	return e;
}

char *php_http_etag_finish(php_http_etag_t *e)
{
	unsigned char digest[128] = {0};
	char *etag = NULL;

	if (!strcasecmp(e->mode, "crc32b")) {
		uint e_ctx;
		memcpy(&e_ctx, e->ctx, 4);
		e_ctx = ntohl(~e_ctx);
		etag = php_http_etag_digest((unsigned char *) &e_ctx, 4);
	} else if ((!strcasecmp(e->mode, "sha1"))) {
		PHP_SHA1Final(digest, e->ctx);
		etag = php_http_etag_digest(digest, 20);
	} else if ((!strcasecmp(e->mode, "md5"))) {
		PHP_MD5Final(digest, e->ctx);
		etag = php_http_etag_digest(digest, 16);
	} else {
#if PHP_HTTP_HAVE_HASH
		const php_hash_ops *eho = NULL;

		if ((eho = php_hash_fetch_ops(e->mode, strlen(e->mode)))) {
			eho->hash_final(digest, e->ctx);
			etag = php_http_etag_digest(digest, eho->digest_size);
		}
#endif
	}

	efree(e->ctx);
	efree(e->mode);
	efree(e);

	return etag;
}

size_t php_http_etag_update(php_http_etag_t *e, const char *data_ptr, size_t data_len)
{
	if (!strcasecmp(e->mode, "crc32b")) {
		uint i, c = *((uint *) e->ctx);
		for (i = 0; i < data_len; ++i) {
			CRC32(c, data_ptr[i]);
		}
		*((uint *) e->ctx) = c;
	} else if ((!strcasecmp(e->mode, "sha1"))) {
		PHP_SHA1Update(e->ctx, (const unsigned char *) data_ptr, data_len);
	} else if ((!strcasecmp(e->mode, "md5"))) {
		PHP_MD5Update(e->ctx, (const unsigned char *) data_ptr, data_len);
	} else {
#if PHP_HTTP_HAVE_HASH
		const php_hash_ops *eho = NULL;

		if ((eho = php_hash_fetch_ops(e->mode, strlen(e->mode)))) {
			eho->hash_update(e->ctx, (const unsigned char *) data_ptr, data_len);
		}
#endif
	}

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

