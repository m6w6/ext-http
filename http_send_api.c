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
#include "php_streams.h"
#include "snprintf.h"

#include "SAPI.h"

#include "php_http.h"
#include "php_http_std_defs.h"
#include "php_http_api.h"
#include "php_http_send_api.h"
#include "php_http_headers_api.h"
#include "php_http_date_api.h"
#include "php_http_cache_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

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
			size_t read = 0;
			php_stream *s = (php_stream *) data;

			if (php_stream_seek(s, begin, SEEK_SET)) {
				return FAILURE;
			}

			buf = (char *) ecalloc(1, HTTP_SENDBUF_SIZE);
			/* read into buf and write out */
			while ((len -= HTTP_SENDBUF_SIZE) >= 0) {
				if (!(read = php_stream_read(s, buf, HTTP_SENDBUF_SIZE))) {
					efree(buf);
					return FAILURE;
				}
				if (read - php_body_write(buf, read TSRMLS_CC)) {
					efree(buf);
					return FAILURE;
				}
				/* ob_flush() && flush() */
				php_end_ob_buffer(1, 1 TSRMLS_CC);
				sapi_flush(TSRMLS_C);
			}

			/* read & write left over */
			if (len) {
				if (read = php_stream_read(s, buf, HTTP_SENDBUF_SIZE + len)) {
					if (read - php_body_write(buf, read TSRMLS_CC)) {
						efree(buf);
						return FAILURE;
					}
				} else {
					efree(buf);
					return FAILURE;
				}
				/* ob_flush() & flush() */
				php_end_ob_buffer(1, 1 TSRMLS_CC);
				sapi_flush(TSRMLS_C);
			}
			efree(buf);
			return SUCCESS;
		}

		case SEND_DATA:
		{
			char *s = (char *) data + begin;

			while ((len -= HTTP_SENDBUF_SIZE) >= 0) {
				if (HTTP_SENDBUF_SIZE - php_body_write(s, HTTP_SENDBUF_SIZE TSRMLS_CC)) {
					return FAILURE;
				}
				s += HTTP_SENDBUF_SIZE;
				/* ob_flush() & flush() */
				php_end_ob_buffer(1, 1 TSRMLS_CC);
				sapi_flush(TSRMLS_C);
			}

			/* write left over */
			if (len) {
				if (HTTP_SENDBUF_SIZE + len - php_body_write(s, HTTP_SENDBUF_SIZE + len TSRMLS_CC)) {
						return FAILURE;
				}
				/* ob_flush() & flush() */
				php_end_ob_buffer(1, 1 TSRMLS_CC);
				sapi_flush(TSRMLS_C);
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
PHP_HTTP_API STATUS _http_send_status_header(int status, const char *header TSRMLS_DC)
{
	sapi_header_line h = {(char *) header, strlen(header), status};
	return sapi_header_op(SAPI_HEADER_REPLACE, &h TSRMLS_CC);
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
		HTTP_G(lmod) = t;

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
		php_error_docref(NULL TSRMLS_CC,E_ERROR,
			"Attempt to send empty ETag (previous: %s)\n", HTTP_G(etag));
		return FAILURE;
	}

	/* remember */
	if (HTTP_G(etag)) {
		efree(HTTP_G(etag));
	}
	HTTP_G(etag) = estrdup(etag);

	etag_header = ecalloc(1, sizeof("ETag: \"\"") + etag_len);
	sprintf(etag_header, "ETag: \"%s\"", etag);
	if (SUCCESS != (status = http_send_header(etag_header))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't send '%s' header", etag_header);
	}
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
	if (SUCCESS != (status = http_send_header(cc_header))) {
		php_error_docref(NULL TSRMLS_CC, E_NOTICE,
			"Could not send '%s' header", cc_header);
	}
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
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Content-Type '%s' doesn't seem to consist of a primary and a secondary part",
			content_type);
		return FAILURE;
	}

	/* remember for multiple ranges */
	if (HTTP_G(ctype)) {
		efree(HTTP_G(ctype));
	}
	HTTP_G(ctype) = estrndup(content_type, ct_len);

	ct_header = ecalloc(1, sizeof("Content-Type: ") + ct_len);
	sprintf(ct_header, "Content-Type: %s", content_type);

	if (SUCCESS != (status = http_send_header(ct_header))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING,
			"Couldn't send '%s' header", ct_header);
	}
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

	if (SUCCESS != (status = http_send_header(cd_header))) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Couldn't send '%s' header", cd_header);
	}
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
			return FAILURE;
		}

		/* Send HTTP 206 Partial Content */
		http_send_status(206);

		/* send content range header */
		snprintf(range_header, 255, "Content-Range: bytes %d-%d/%d", **begin, **end, size);
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
		snprintf(bound, 22, "--%d%0.9f", time(NULL), php_combined_lcg(TSRMLS_C));
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
				HTTP_CRLF "Content-Range: bytes %ld-%ld/%ld"
				HTTP_CRLF
				HTTP_CRLF,

				bound,
				HTTP_G(ctype) ? HTTP_G(ctype) : "application/x-octetstream",
				**begin,
				**end,
				size
			);

			php_body_write(preface, strlen(preface) TSRMLS_CC);
			http_send_chunk(data, **begin, **end + 1, mode);
		}

		/* write boundary once more */
		php_body_write(HTTP_CRLF, sizeof(HTTP_CRLF) - 1 TSRMLS_CC);
		php_body_write(bound, strlen(bound) TSRMLS_CC);
		php_body_write("--", 2 TSRMLS_CC);

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
	if (cache_etag = HTTP_G(etag_started)) {
		/* interrupt */
		HTTP_G(etag_started) = 0;
		/* never ever use the output to compute the ETag if http_send() is used */
		php_end_ob_buffers(0 TSRMLS_CC);
	}

	zend_hash_init(&ranges, 0, NULL, ZVAL_PTR_DTOR, 0);
	range_status = http_get_request_ranges(&ranges, data_size);

	if (range_status == RANGE_ERR) {
		zend_hash_destroy(&ranges);
		http_send_status(416);
		return FAILURE;
	}

	/* Range Request - only send ranges if entity hasn't changed */
	if (	range_status == RANGE_OK &&
			http_etag_match_ex("HTTP_IF_MATCH", HTTP_G(etag), 0) &&
			http_modified_match_ex("HTTP_IF_UNMODIFIED_SINCE", HTTP_G(lmod), 0)) {
		STATUS result = http_send_ranges(&ranges, data_ptr, data_size, data_mode);
		zend_hash_destroy(&ranges);
		return result;
	}

	zend_hash_destroy(&ranges);

	/* send 304 Not Modified if etag matches */
	if (cache_etag) {
		char *etag = NULL;
		int etag_match = 0;

		if (!(etag = http_etag(data_ptr, data_size, data_mode))) {
			return FAILURE;
		}

		http_send_etag(etag, 32);
		etag_match = http_etag_match("HTTP_IF_NONE_MATCH", etag);
		efree(etag);

		if (etag_match) {
			return http_send_status(304);
		}
	}

	/* send 304 Not Modified if last modified matches */
	if (http_modified_match("HTTP_IF_MODIFIED_SINCE", HTTP_G(lmod))) {
		return http_send_status(304);
	}

	/* send full entity */
	return http_send_chunk(data_ptr, 0, data_size, data_mode);
}
/* }}} */

/* {{{ STATUS http_send_stream(php_stream *) */
PHP_HTTP_API STATUS _http_send_stream_ex(php_stream *file, zend_bool close_stream TSRMLS_DC)
{
	STATUS status;

	if ((!file) || php_stream_stat(file, &HTTP_G(ssb))) {
		return FAILURE;
	}

	status = http_send(file, HTTP_G(ssb).sb.st_size, SEND_RSRC);

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

