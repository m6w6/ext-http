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
#include "ext/standard/base64.h"

#include "SAPI.h"

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_std_defs.h"
#include "php_http_send_api.h"

/*
 * TODO:
 *  - Digest Auth
 */

/* {{{ STATUS http_auth_header(char *, char*) */
PHP_HTTP_API STATUS _http_auth_header(const char *type, const char *realm TSRMLS_DC)
{
	char realm_header[1024] = {0};
	snprintf(realm_header, 1023, "WWW-Authenticate: %s realm=\"%s\"", type, realm);
	return http_send_status_header(401, realm_header);
}
/* }}} */

/* {{{ STATUS http_auth_credentials(char **, char **) */
PHP_HTTP_API STATUS _http_auth_credentials(char **user, char **pass TSRMLS_DC)
{
	if (strncmp(sapi_module.name, "isapi", 5)) {
		zval *zuser, *zpass;

		HTTP_GSC(zuser, "PHP_AUTH_USER", FAILURE);
		HTTP_GSC(zpass, "PHP_AUTH_PW", FAILURE);

		*user = estrndup(Z_STRVAL_P(zuser), Z_STRLEN_P(zuser));
		*pass = estrndup(Z_STRVAL_P(zpass), Z_STRLEN_P(zpass));

		return SUCCESS;
	} else {
		zval *zauth = NULL;
		HTTP_GSC(zauth, "HTTP_AUTHORIZATION", FAILURE);
		{
			char *decoded, *colon;
			int decoded_len;
			decoded = php_base64_decode(Z_STRVAL_P(zauth), Z_STRLEN_P(zauth),
				&decoded_len);

			if (colon = strchr(decoded + 6, ':')) {
				*user = estrndup(decoded + 6, colon - decoded - 6);
				*pass = estrndup(colon + 1, decoded + decoded_len - colon - 6 - 1);

				return SUCCESS;
			} else {
				return FAILURE;
			}
		}
	}
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

