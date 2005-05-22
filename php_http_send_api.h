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

#ifndef PHP_HTTP_SEND_API_H
#define PHP_HTTP_SEND_API_H

#include "SAPI.h"
#include "php_streams.h"
#include "php_http_std_defs.h"

typedef enum {
	SEND_DATA,
	SEND_RSRC
} http_send_mode;

#define http_send_status(s) sapi_header_op(SAPI_HEADER_SET_STATUS, (void *) (s) TSRMLS_CC)
#define http_send_header(h) http_send_status_header(0, (h))
#define http_send_header_ex(h, r) http_send_status_header_ex(0, (h), (r))
#define http_send_status_header(s, h) _http_send_status_header_ex((s), (h), 1 TSRMLS_CC)
#define http_send_status_header_ex(s, h, r) _http_send_status_header_ex((s), (h), (r) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_status_header_ex(int status, const char *header, zend_bool replace TSRMLS_DC);

#define http_send_last_modified(t) _http_send_last_modified((t) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_last_modified(time_t t TSRMLS_DC);

#define http_send_etag(e, l) _http_send_etag((e), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_etag(const char *etag, size_t etag_len TSRMLS_DC);

#define http_send_cache_control(c, l) _http_send_cache_control((c), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_cache_control(const char *cache_control, size_t cc_len TSRMLS_DC);

#define http_send_content_type(c, l) _http_send_content_type((c), (l) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_content_type(const char *content_type, size_t ct_len TSRMLS_DC);

#define http_send_content_disposition(f, l, i) _http_send_content_disposition((f), (l), (i) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_content_disposition(const char *filename, size_t f_len, zend_bool send_inline TSRMLS_DC);

#define http_send_ranges(r, d, s, m) _http_send_ranges((r), (d), (s), (m) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_ranges(HashTable *ranges, const void *data, size_t size, http_send_mode mode TSRMLS_DC);

#define http_send_data(d, l) http_send((d), (l), SEND_DATA)
#define http_send(d, s, m) _http_send((d), (s), (m) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send(const void *data, size_t data_size, http_send_mode mode TSRMLS_DC);

#define http_send_file(f) http_send_stream_ex(php_stream_open_wrapper(f, "rb", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL), 1)
#define http_send_stream(s) http_send_stream_ex((s), 0)
#define http_send_stream_ex(s, c) _http_send_stream_ex((s), (c) TSRMLS_CC)
PHP_HTTP_API STATUS _http_send_stream_ex(php_stream *s, zend_bool close_stream TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

