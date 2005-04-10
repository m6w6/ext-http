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

#ifndef PHP_HTTP_HEADERS_API_H
#define PHP_HTTP_HEADERS_API_H

#include "php_http_std_defs.h"

typedef enum {
	RANGE_OK,
	RANGE_NO,
	RANGE_ERR
} http_range_status;

#define http_parse_headers(h, l, a) _http_parse_headers_ex((h), (l), Z_ARRVAL_P(a), 1 TSRMLS_CC)
#define http_parse_headers_ex(h, l, ht, p) _http_parse_headers_ex((h), (l), (ht), (p) TSRMLS_CC)
PHP_HTTP_API STATUS _http_parse_headers_ex(const char *header, size_t header_len, HashTable *headers, zend_bool prettify TSRMLS_DC);

#define http_parse_cookie(c, ht) _http_parse_cookie((c), (ht) TSRMLS_CC)
PHP_HTTP_API STATUS _http_parse_cookie(const char *cookie, HashTable *values TSRMLS_DC);

#define http_get_request_headers(h) _http_get_request_headers_ex(Z_ARRVAL_P(h), 1 TSRMLS_CC)
#define http_get_request_headers_ex(h, p) _http_get_request_headers_ex((h), (s) TSRMLS_CC)
PHP_HTTP_API void _http_get_request_headers_ex(HashTable *headers, zend_bool prettify TSRMLS_DC);

#define http_negotiate_language(zsupported, def) http_negotiate_language_ex(Z_ARRVAL_P(zsupported), (def))
#define http_negotiate_language_ex(supported, def) http_negotiate_q("HTTP_ACCEPT_LANGUAGE", (supported), (def))
#define http_negotiate_charset(zsupported, def) http_negotiate_charset_ex(Z_ARRVAL_P(zsupported), (def))
#define http_negotiate_charset_ex(supported, def)  http_negotiate_q("HTTP_ACCEPT_CHARSET", (supported), (def))
#define http_negotiate_q(e, s, d) _http_negotiate_q((e), (s), (d) TSRMLS_CC)
PHP_HTTP_API char *_http_negotiate_q(const char *entry, const HashTable *supported, const char *def TSRMLS_DC);

#define http_get_request_ranges(r, l) _http_get_request_ranges((r), (l) TSRMLS_CC)
PHP_HTTP_API http_range_status _http_get_request_ranges(HashTable *ranges, size_t length TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

