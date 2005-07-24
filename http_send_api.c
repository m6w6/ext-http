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

#include "SAPI.h"
#include "php_streams.h"
#include "ext/standard/php_lcg.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_date_api.h"
#include "php_http_send_api.h"
#include "php_http_headers_api.h"
#include "php_http_date_api.h"
#include "php_http_cache_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

#define http_flush() _http_flush(TSRMLS_C)
/* {{{ static inline void http_flush() */
static inline void _http_flush(TSRMLS_D)
{
	php_end_ob_buffer(1, 1 TSRMLS_CC);
	sapi_flush(TSRMLS_C);
}
/* }}} */

#define http_sleep() _http_sleep(TSRMLS_C)
/* {{{ static inline void http_sleep() */
static inline void _http_sleep(TSRMLS_D)
{
#define HTTP_MSEC(s) (s * 1000)
#define HTTP_USEC(s) (HTTP_MSEC(s) * 1000)
#define HTTP_NSEC(s) (HTTP_USEC(s) * 1000)
#define HTTP_NANOSEC (1000 * 1000 * 1000)
#define HTTP_DIFFSEC (0.001)

	if (HTTP_G(send).throttle_delay >= HTTP_DIFFSEC) {
#if defined(PHP_WIN32)
		Sleep((DWORD) HTTP_MSEC(HTTP_G(send).throttle_delay));
#elif defined(HAVE_USLEEP)
		usleep(HTTP_USEC(HTTP_G(send).throttle_delay));
#elif defined(HAVE_NANOSLEEP)
		struct timespec req, rem;

		req.tv_sec = (time_t) HTTP_G(send).throttle_delay;
		req.tv_nsec = HTTP_NSEC(HTTP_G(send).throttle_delay) % HTTP_NANOSEC;

		while (nanosleep(&req, &rem) && (errno == EINTR) && (HTTP_NSEC(rem.tv_sec) + rem.tv_nsec) > HTTP_NSEC(HTTP_DIFFSEC))) {
			req.tv_sec = rem.tv_sec;
			req.tv_nsec = rem.tv_nsec;
		}
#endif
	}
}
/* }}} */

#define HTTP_CHUNK_AVAIL(len) ((len -= HTTP_G(send).buffer_size) >= 0)
#define HTTP_CHUNK_WRITE(data, l, dofree, dosleep) \
	{ \
		long size = (long) l; \
 \
		if ((1 > size) || (size - PHPWRITE(data, size))) { \
			if (dofree) { \
				efree(data); \
			} \
			return FAILURE; \
		} \
 \
		http_flush(); \
		if (dosleep) { \
			http_sleep(); \
		} \
	}

#define http_send_chunk(d, b, e, m) _http_send_chunk((d), (b), (e), (m) TSRMLS_CC)
/* {{{ static STATUS http_send_chunk(const void *, size_t, size_t, http_send_mode) */
static STATUS _http_send_chunk(const void *data, size_t begin, size_t end, http_send_mode mode TSRMLS_DC)
{
	long len = end - begin;

	switch (mode)
	{
		case SEND_RSRC:
		{
			char *buf;
			php_stream *s = (php_stream *) data;

			if (php_stream_seek(s, begin, SEEK_SET)) {
				return FAILURE;
			}

			buf = emalloc(HTTP_G(send).buffer_size);

			while (HTTP_CHUNK_AVAIL(len)) {
				HTTP_CHUNK_WRITE(buf, php_stream_read(s, buf, HTTP_G(send).buffer_size), 1, 1);
			}

			/* read & write left over */
			if (len) {
				HTTP_CHUNK_WRITE(buf, php_stream_read(s, buf, HTTP_G(send).buffer_size + len), 1, 0);
			}

			efree(buf);
			return SUCCESS;
		}

		case SEND_DATA:
		{
			char *s = (char *) data + begin;

			while (HTTP_CHUNK_AVAIL(len)) {
				HTTP_CHUNK_WRITE(s, HTTP_G(send).buffer_size, 0, 1);
				s += HTTP_G(send).buffer_size;
			}

			/* write left over */
			if (len) {
				HTTP_CHUNK_WRITE(s, HTTP_G(send).buffer_size + len, 0, 0);
			}

			return SUCCESS;
		}

		default:
			return FAILURE;
		break;
	}
}
/* }}} */


/* {{{ STATUS http_send_status_header(int, char *) */
PHP_HTTP_API STATUS _http_send_status_header_ex(int status, const char *header, zend_bool replace TSRMLS_DC)
{
	STATUS ret;
	sapi_header_line h = {(char *) header, header ? strlen(header) : 0, status};
	if (SUCCESS != (ret = sapi_header_op(replace ? SAPI_HEADER_REPLACE : SAPI_HEADER_ADD, &h TSRMLS_CC))) {
		http_error_ex(E_WARNING, HTTP_E_HEADER, "Could not send header: %s (%d)", header, status);
	}
	return ret;
}
/* }}} */

/* {{{ STATUS http_send_last_modified(int) */
PHP_HTTP_API STATUS _http_send_last_modified(time_t t TSRMLS_DC)
{
	char *date = NULL;
	if (date = http_date(t)) {
		char modified[96] = "Last-Modified: ";
		strcat(modified, date);
		efree(date);

		/* remember */
		HTTP_G(send).last_modified = t;

		return http_send_header(modified);
	}
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_send_etag(char *, size_t) */
PHP_HTTP_API STATUS _http_send_etag(const char *etag, size_t etag_len TSRMLS_DC)
{
	STATUS status;
	char *etag_header;

	if (!etag_len){
		http_error_ex(E_WARNING, HTTP_E_HEADER, "Attempt to send empty ETag (previous: %s)\n", HTTP_G(send).unquoted_etag);
		return FAILURE;
	}

	/* remember */
	STR_FREE(HTTP_G(send).unquoted_etag);
	HTTP_G(send).unquoted_etag = estrdup(etag);

	etag_header = ecalloc(1, sizeof("ETag: \"\"") + etag_len);
	sprintf(etag_header, "ETag: \"%s\"", etag);
	status = http_send_header(etag_header);
	efree(etag_header);
	return status;
}
/* }}} */

/* {{{ STATUS http_send_cache_control(char *, size_t) */
PHP_HTTP_API STATUS _http_send_cache_control(const char *cache_control, size_t cc_len TSRMLS_DC)
{
	STATUS status;
	char *cc_header = ecalloc(1, sizeof("Cache-Control: ") + cc_len);

	sprintf(cc_header, "Cache-Control: %s", cache_control);
	status = http_send_header(cc_header);
	efree(cc_header);
	return status;
}
/* }}} */

/* {{{ STATUS http_send_content_type(char *, size_t) */
PHP_HTTP_API STATUS _http_send_content_type(const char *content_type, size_t ct_len TSRMLS_DC)
{
	STATUS status;
	char *ct_header;

	if (!strchr(content_type, '/')) {
		http_error_ex(E_WARNING, HTTP_E_PARAM, "Content-Type '%s' doesn't seem to consist of a primary and a secondary part", content_type);
		return FAILURE;
	}

	/* remember for multiple ranges */
	STR_FREE(HTTP_G(send).content_type);
	HTTP_G(send).content_type = estrndup(content_type, ct_len);

	ct_header = ecalloc(1, sizeof("Content-Type: ") + ct_len);
	sprintf(ct_header, "Content-Type: %s", content_type);
	status = http_send_header(ct_header);
	efree(ct_header);
	return status;
}
/* }}} */

/* {{{ STATUS http_send_content_disposition(char *, size_t, zend_bool) */
PHP_HTTP_API STATUS _http_send_content_disposition(const char *filename, size_t f_len, zend_bool send_inline TSRMLS_DC)
{
	STATUS status;
	char *cd_header;

	if (send_inline) {
		cd_header = ecalloc(1, sizeof("Content-Disposition: inline; filename=\"\"") + f_len);
		sprintf(cd_header, "Content-Disposition: inline; filename=\"%s\"", filename);
	} else {
		cd_header = ecalloc(1, sizeof("Content-Disposition: attachment; filename=\"\"") + f_len);
		sprintf(cd_header, "Content-Disposition: attachment; filename=\"%s\"", filename);
	}

	status = http_send_header(cd_header);
	efree(cd_header);
	return status;
}
/* }}} */

/* {{{ STATUS http_send_ranges(HashTable *, void *, size_t, http_send_mode) */
PHP_HTTP_API STATUS _http_send_ranges(HashTable *ranges, const void *data, size_t size, http_send_mode mode TSRMLS_DC)
{
	long **begin, **end;
	zval **zrange;

	/* single range */
	if (zend_hash_num_elements(ranges) == 1) {
		char range_header[256] = {0};

		if (SUCCESS != zend_hash_index_find(ranges, 0, (void **) &zrange) ||
			SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(zrange), 0, (void **) &begin) ||
			SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(zrange), 1, (void **) &end)) {
			http_send_status(500);
			return FAILURE;
		}

		/* Send HTTP 206 Partial Content */
		http_send_status(206);

		/* send content range header */
		snprintf(range_header, 255, "Content-Range: bytes %ld-%ld/%lu", **begin, **end, (ulong) size);
		http_send_header(range_header);

		/* send requested chunk */
		return http_send_chunk(data, **begin, **end + 1, mode);
	}

	/* multi range */
	else {
		char bound[23] = {0}, preface[1024] = {0},
			multi_header[68] = "Content-Type: multipart/byteranges; boundary=";

		/* Send HTTP 206 Partial Content */
		http_send_status(206);

		/* send multipart/byteranges header */
		snprintf(bound, 22, "--%lu%0.9f", (ulong) time(NULL), php_combined_lcg(TSRMLS_C));
		strncat(multi_header, bound + 2, 21);
		http_send_header(multi_header);

		/* send each requested chunk */
		FOREACH_HASH_VAL(ranges, zrange) {
			if (SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(zrange), 0, (void **) &begin) ||
				SUCCESS != zend_hash_index_find(Z_ARRVAL_PP(zrange), 1, (void **) &end)) {
				break;
			}

			snprintf(preface, 1023,
				HTTP_CRLF "%s"
				HTTP_CRLF "Content-Type: %s"
				HTTP_CRLF "Content-Range: bytes %ld-%ld/%lu"
				HTTP_CRLF
				HTTP_CRLF,

				bound,
				HTTP_G(send).content_type ? HTTP_G(send).content_type : "application/x-octetstream",
				**begin,
				**end,
				(ulong) size
			);

			PHPWRITE(preface, strlen(preface));
			http_send_chunk(data, **begin, **end + 1, mode);
		}

		/* write boundary once more */
		PHPWRITE(HTTP_CRLF, lenof(HTTP_CRLF));
		PHPWRITE(bound, strlen(bound));
		PHPWRITE("--", lenof("--"));

		return SUCCESS;
	}
}
/* }}} */

/* {{{ STATUS http_send(void *, size_t, http_send_mode) */
PHP_HTTP_API STATUS _http_send(const void *data_ptr, size_t data_size, http_send_mode data_mode TSRMLS_DC)
{
	HashTable ranges;
	http_range_status range_status;
	int cache_etag = 0;

	if (!data_ptr) {
		return FAILURE;
	}
	if (!data_size) {
		return SUCCESS;
	}

	/* stop on-the-fly etag generation */
	if (cache_etag = HTTP_G(etag).started) {
		/* interrupt ob_etaghandler */
		HTTP_G(etag).started = 0;
	}

	/* enable partial dl and resume */
	http_send_header("Accept-Ranges: bytes");

	zend_hash_init(&ranges, 0, NULL, ZVAL_PTR_DTOR, 0);
	range_status = http_get_request_ranges(&ranges, data_size);

	if (range_status == RANGE_ERR) {
		zend_hash_destroy(&ranges);
		http_send_status(416);
		return FAILURE;
	}

	/* Range Request - only send ranges if entity hasn't changed */
	if (	range_status == RANGE_OK &&
			http_match_etag_ex("HTTP_IF_MATCH", HTTP_G(send).unquoted_etag, 0) &&
			http_match_last_modified_ex("HTTP_IF_UNMODIFIED_SINCE", HTTP_G(send).last_modified, 0)) {
		STATUS result = http_send_ranges(&ranges, data_ptr, data_size, data_mode);
		zend_hash_destroy(&ranges);
		return result;
	}

	zend_hash_destroy(&ranges);

	/* send 304 Not Modified if etag matches - DON'T return on ETag generation failure */
	if (cache_etag) {
		char *etag = NULL;

		if (!(etag = http_etag(data_ptr, data_size, data_mode))) {
			http_error(E_NOTICE, HTTP_E_PARAM, "Failed to generate ETag for data source");
		} else {
			http_send_etag(etag, 32);
			if (http_match_etag("HTTP_IF_NONE_MATCH", etag)) {
				return http_cache_exit_ex(etag, 1, 1);
			}
			efree(etag);
		}
	}

	/* send 304 Not Modified if last modified matches */
	if (http_match_last_modified("HTTP_IF_MODIFIED_SINCE", HTTP_G(send).last_modified)) {
		return http_cache_exit_ex(http_date(HTTP_G(send).last_modified), 0, 1);
	}

	/* send full entity */
	return http_send_chunk(data_ptr, 0, data_size, data_mode);
}
/* }}} */

/* {{{ STATUS http_send_stream(php_stream *) */
PHP_HTTP_API STATUS _http_send_stream_ex(php_stream *file, zend_bool close_stream TSRMLS_DC)
{
	STATUS status;
	php_stream_statbuf ssb;

	if ((!file) || php_stream_stat(file, &ssb)) {
		return FAILURE;
	}

	status = http_send(file, ssb.sb.st_size, SEND_RSRC);

	if (close_stream) {
		php_stream_close(file);
	}

	return status;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */

