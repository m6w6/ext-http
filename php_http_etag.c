#include "php_http.h"

#ifdef PHP_HTTP_HAVE_HASH
#	include "php_hash.h"
#endif

#include <ext/standard/crc32.h>
#include <ext/standard/sha1.h>
#include <ext/standard/md5.h>

PHP_HTTP_API void *php_http_etag_init(TSRMLS_D)
{
	void *ctx = NULL;
	char *mode = PHP_HTTP_G->env.etag_mode;

#ifdef PHP_HTTP_HAVE_HASH
	const php_hash_ops *eho = NULL;

	if (mode && (eho = php_hash_fetch_ops(mode, strlen(mode)))) {
		ctx = emalloc(eho->context_size);
		eho->hash_init(ctx);
	} else
#endif
	if (mode && ((!strcasecmp(mode, "crc32")) || (!strcasecmp(mode, "crc32b")))) {
		ctx = emalloc(sizeof(uint));
		*((uint *) ctx) = ~0;
	} else if (mode && !strcasecmp(mode, "sha1")) {
		PHP_SHA1Init(ctx = emalloc(sizeof(PHP_SHA1_CTX)));
	} else {
		PHP_MD5Init(ctx = emalloc(sizeof(PHP_MD5_CTX)));
	}

	return ctx;
}

PHP_HTTP_API char *php_http_etag_finish(void *ctx TSRMLS_DC)
{
	unsigned char digest[128] = {0};
	char *etag = NULL, *mode = PHP_HTTP_G->env.etag_mode;

#ifdef PHP_HTTP_HAVE_HASH
	const php_hash_ops *eho = NULL;

	if (mode && (eho = php_hash_fetch_ops(mode, strlen(mode)))) {
		eho->hash_final(digest, ctx);
		etag = php_http_etag_digest(digest, eho->digest_size);
	} else
#endif
	if (mode && ((!strcasecmp(mode, "crc32")) || (!strcasecmp(mode, "crc32b")))) {
		*((uint *) ctx) = ~*((uint *) ctx);
		etag = php_http_etag_digest((const unsigned char *) ctx, sizeof(uint));
	} else if (mode && (!strcasecmp(mode, "sha1"))) {
		PHP_SHA1Final(digest, ctx);
		etag = php_http_etag_digest(digest, 20);
	} else {
		PHP_MD5Final(digest, ctx);
		etag = php_http_etag_digest(digest, 16);
	}
	efree(ctx);

	return etag;
}

PHP_HTTP_API size_t php_http_etag_update(void *ctx, const char *data_ptr, size_t data_len TSRMLS_DC)
{
	char *mode = PHP_HTTP_G->env.etag_mode;
#ifdef PHP_HTTP_HAVE_HASH
	const php_hash_ops *eho = NULL;

	if (mode && (eho = php_hash_fetch_ops(mode, strlen(mode)))) {
		eho->hash_update(ctx, (const unsigned char *) data_ptr, data_len);
	} else
#endif
	if (mode && ((!strcasecmp(mode, "crc32")) || (!strcasecmp(mode, "crc32b")))) {
		uint i, c = *((uint *) ctx);
		for (i = 0; i < data_len; ++i) {
			CRC32(c, data_ptr[i]);
		}
		*((uint *)ctx) = c;
	} else if (mode && (!strcasecmp(mode, "sha1"))) {
		PHP_SHA1Update(ctx, (const unsigned char *) data_ptr, data_len);
	} else {
		PHP_MD5Update(ctx, (const unsigned char *) data_ptr, data_len);
	}

	return data_len;
}

