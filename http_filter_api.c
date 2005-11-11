/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "php.h"

#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_filter_api.h"

#include "phpstr/phpstr.h"

#include "php_streams.h"

#ifndef HTTP_DEBUG_FILTERS
#	define HTTP_DEBUG_FILTERS 0
#endif

/*
 * TODO: phpstr is not persistent aware
 */

typedef enum {
	HFS_HEX = 0,
	HFS_DATA,
} http_filter_status;

typedef struct {
	phpstr buffer;
	size_t wanted;
	int eollen;
	int passon;
	http_filter_status status;
} http_filter_buffer;

#define PHP_STREAM_FILTER_OP_FILTER_PARAMS \
	php_stream *stream, \
	php_stream_filter *this, \
	php_stream_bucket_brigade *buckets_in, \
	php_stream_bucket_brigade *buckets_out, \
	size_t *bytes_consumed, int flags \
	TSRMLS_DC
#define PHP_STREAM_FILTER_OP_FILTER(function) \
	static php_stream_filter_status_t function(PHP_STREAM_FILTER_OP_FILTER_PARAMS)

#define NEW_BUCKET(data, length) \
	php_stream_bucket_append(buckets_out, php_stream_bucket_new(stream, pestrndup(data, length, this->is_persistent), (length), 1, this->is_persistent TSRMLS_CC) TSRMLS_CC);

inline void *pestrndup(const char *s, size_t l, int p)
{
	void *d = pemalloc(l + 1, p);
	if (d) {
		memcpy(d, s, l);
		((char *) d)[l] = 0;
	}
	return d;
}

PHP_STREAM_FILTER_OP_FILTER(http_filter_chunked_decode)
{
	php_stream_bucket *ptr, *nxt;
	http_filter_buffer *buffer = (http_filter_buffer *) (this->abstract);
	
	if (bytes_consumed) {
		*bytes_consumed = 0;
	}
	
	if (!buckets_in->head) {
		return PSFS_FEED_ME;
	}
	
#if HTTP_DEBUG_FILTERS
	fprintf(stderr, "Reading in bucket buffers ");
#endif
	
	/* fetch available bucket data */
	for (ptr = buckets_in->head; ptr; ptr = nxt) {
		nxt = ptr->next;
		phpstr_append(PHPSTR(buffer), ptr->buf, ptr->buflen);
		php_stream_bucket_unlink(ptr TSRMLS_CC);
		php_stream_bucket_delref(ptr TSRMLS_CC);
	
#if HTTP_DEBUG_FILTERS
		fprintf(stderr, ".");
#endif
	}
	if (bytes_consumed) {
		*bytes_consumed = PHPSTR_LEN(buffer);
	}
	phpstr_fix(PHPSTR(buffer));

#if HTTP_DEBUG_FILTERS
	fprintf(stderr, " done\nCurrent buffer length: %lu bytes\n", PHPSTR_LEN(buffer));
#endif
	
	buffer->passon = 0;
	while (1) {
		if (buffer->status == HFS_HEX) {
			const char *eol;
			char *stop;
			ulong clen;
			
#if HTTP_DEBUG_FILTERS
			fprintf(stderr, "Status HFS_HEX: ");
#endif
			
			if (!(eol = http_locate_eol(PHPSTR_VAL(buffer), &buffer->eollen))) {
#if HTTP_DEBUG_FILTERS
				fprintf(stderr, "return PFSF_FEED_ME (no eol)\n");
#endif
				return buffer->passon ? PSFS_PASS_ON : PSFS_FEED_ME;
			}
			if (!(clen = strtoul(PHPSTR_VAL(buffer), &stop, 16))) {
#if HTTP_DEBUG_FILTERS
				fprintf(stderr, "return PFSF_FEED_ME (no len)\n");
#endif
				phpstr_dtor(PHPSTR(buffer));
				return buffer->passon ? PSFS_PASS_ON : PSFS_FEED_ME;
			}
			
			buffer->status = HFS_DATA;
			buffer->wanted = clen;
			phpstr_cut(PHPSTR(buffer), 0, eol + buffer->eollen - PHPSTR_VAL(buffer));
			
#if HTTP_DEBUG_FILTERS
			fprintf(stderr, "read %lu bytes chunk size\n", buffer->wanted);
#endif
		}
		
#if HTTP_DEBUG_FILTERS
		fprintf(stderr, "Current status: %s\n", buffer->status == HFS_DATA?"HFS_DATA":"HFS_HEX");
		fprintf(stderr, "Current buffer length: %lu bytes\n", PHPSTR_LEN(buffer));
#endif
		
		if (buffer->status == HFS_DATA && buffer->wanted > 0 && buffer->wanted <= PHPSTR_LEN(buffer)) {
		
#if HTTP_DEBUG_FILTERS
			fprintf(stderr, "Passing on %lu(%lu) bytes\n", buffer->wanted, PHPSTR_LEN(buffer));
#endif

			NEW_BUCKET(PHPSTR_VAL(buffer), buffer->wanted);
			phpstr_cut(PHPSTR(buffer), 0, buffer->wanted + buffer->eollen);
			buffer->wanted = 0;
			buffer->eollen = 0;
			buffer->passon = 1;
			buffer->status = HFS_HEX;
			continue;
		}
		return buffer->passon ? PSFS_PASS_ON : PSFS_FEED_ME;
	}
	
	return PSFS_FEED_ME;
}

static void http_filter_chunked_decode_dtor(php_stream_filter *this TSRMLS_DC)
{
	http_filter_buffer *b = (http_filter_buffer *) (this->abstract);
	
	phpstr_dtor(PHPSTR(b));
	pefree(b, this->is_persistent);
}

PHP_STREAM_FILTER_OP_FILTER(http_filter_chunked_encode)
{
	phpstr buf;
	php_stream_bucket *ptr, *nxt;
	
	if (bytes_consumed) {
		*bytes_consumed = 0;
	}
	
	if (!buckets_in->head) {
		return PSFS_FEED_ME;
	}
	
	phpstr_init(&buf);
	for (ptr = buckets_in->head; ptr; ptr = nxt) {
		if (bytes_consumed) {
			*bytes_consumed += ptr->buflen;
		}
		nxt = ptr->next;
		
		phpstr_appendf(&buf, "%x" HTTP_CRLF, ptr->buflen);
		phpstr_append(&buf, ptr->buf, ptr->buflen);
		phpstr_appends(&buf, HTTP_CRLF);
		NEW_BUCKET(PHPSTR_VAL(&buf), PHPSTR_LEN(&buf));
		PHPSTR_LEN(&buf) = 0;
		
		php_stream_bucket_unlink(ptr TSRMLS_CC);
		php_stream_bucket_delref(ptr TSRMLS_CC);
	}
	phpstr_dtor(&buf);
	
	if (flags & PSFS_FLAG_FLUSH_CLOSE) {
		NEW_BUCKET("0"HTTP_CRLF, lenof("0"HTTP_CRLF));
	}
	
	return PSFS_PASS_ON;
}

static php_stream_filter_ops http_filter_ops_chunked_decode = {
	http_filter_chunked_decode,
	http_filter_chunked_decode_dtor,
	"http.chunked_decode"
};

static php_stream_filter_ops http_filter_ops_chunked_encode = {
	http_filter_chunked_encode,
	NULL,
	"http.chunked_encode"
};

static php_stream_filter *http_filter_create(const char *name, zval *params, int p TSRMLS_DC)
{
	php_stream_filter *f = NULL;
	void *b = NULL;
	
	if (!strcasecmp(name, "http.chunked_decode")) {
		if (b = pecalloc(1, sizeof(http_filter_buffer), p)) {
			phpstr_init(PHPSTR(b));
			if (!(f = php_stream_filter_alloc(&http_filter_ops_chunked_decode, b, p))) {
				pefree(b, p);
			}
		}
	} else
	if (!strcasecmp(name, "http.chunked_encode")) {
		f = php_stream_filter_alloc(&http_filter_ops_chunked_encode, NULL, p);
	}
	
	return f;
}

php_stream_filter_factory http_filter_factory = {
	http_filter_create
};

PHP_MINIT_FUNCTION(http_filter)
{
	php_stream_filter_register_factory("http.*", &http_filter_factory TSRMLS_CC);
	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

