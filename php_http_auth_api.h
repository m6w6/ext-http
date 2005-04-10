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

#ifndef PHP_HTTP_AUTH_API_H
#define PHP_HTTP_AUTH_API_H

#include "php_http_std_defs.h"

#define http_auth_credentials(u, p) _http_auth_credentials((u), (p) TSRMLS_CC)
PHP_HTTP_API STATUS _http_auth_credentials(char **user, char **pass TSRMLS_DC);

#define http_auth_header(t, r) _http_auth_header((t), (r) TSRMLS_CC)
PHP_HTTP_API STATUS _http_auth_header(const char *type, const char *realm TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

