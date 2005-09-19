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

#include "php_http.h"
#include "php_http_api.h"
#include "php_http_std_defs.h"
#include "php_http_info_api.h"

#include <ctype.h>

ZEND_EXTERN_MODULE_GLOBALS(http);

PHP_HTTP_API void _http_info_default_callback(void **nothing, HashTable **headers, http_info *info TSRMLS_DC)
{
	zval array;
	
	INIT_ZARR(array, *headers);
	
	switch (info->type)
	{
		case IS_HTTP_REQUEST:
			add_assoc_string(&array, "Request Method", HTTP_INFO(info).request.method, 1);
			add_assoc_string(&array, "Request Uri", HTTP_INFO(info).request.URI, 1);
		break;
		
		case IS_HTTP_RESPONSE:
			add_assoc_long(&array, "Response Code", (long) HTTP_INFO(info).response.code);
			add_assoc_string(&array, "Response Status", HTTP_INFO(info).response.status, 1);
		break;
	}
}

PHP_HTTP_API void _http_info_dtor(http_info *info)
{
	http_info_t *i = (http_info_t *) info;
	
	switch (info->type)
	{
		case IS_HTTP_REQUEST:
			STR_SET(i->request.method, NULL);
			STR_SET(i->request.URI, NULL);
		break;
		
		case IS_HTTP_RESPONSE:
			STR_SET(i->response.status, NULL);
		break;
		
		default:
		break;
	}
}

PHP_HTTP_API STATUS _http_info_parse_ex(const char *pre_header, http_info *info, zend_bool silent TSRMLS_DC)
{
	const char *end, *http;
	
	/* sane parameter */
	if ((!pre_header) || (!*pre_header)) {
		if (!silent) {
			http_error(HE_WARNING, HTTP_E_MALFORMED_HEADERS, "Empty pre-header HTTP info");
		}
		return FAILURE;
	}
	
	/* where's the end of the line */
	if (!(end = http_locate_eol(pre_header, NULL))) {
		end = pre_header + strlen(pre_header);
	}
	
	/* there must be HTTP/1.x in the line
	 * and nothing than SPACE or NUL after HTTP/1.x 
	 */
	if (	(!(http = strstr(pre_header, "HTTP/1."))) || 
			(!(http < end)) ||
			(!isdigit(http[lenof("HTTP/1.")])) ||
			(http[lenof("HTTP/1.1")] && (!isspace(http[lenof("HTTP/1.1")])))) {
		if (!silent) {
			http_error(HE_WARNING, HTTP_E_MALFORMED_HEADERS, "Invalid or missing HTTP/1.x protocol identification");
		}
		return FAILURE;
	}

#if 0
	{
		char *line = estrndup(pre_header, end - pre_header);
		fprintf(stderr, "http_parse_info('%s')\n", line);
		efree(line);
	}
#endif

	info->http.version = atof(http + lenof("HTTP/"));
	
	/* is response */
	if (pre_header == http) {
		char *status = NULL;
		const char *code = http + sizeof("HTTP/1.1");
		
		info->type = IS_HTTP_RESPONSE;
		HTTP_INFO(info).response.code = (code && (end > code)) ? strtol(code, &status, 10) : 0;
		HTTP_INFO(info).response.status = (status && (end > ++status)) ? estrndup(status, end - status) : ecalloc(1,1);
		
		return SUCCESS;
	}
	
	/* is request */
	else {
		const char *url = strchr(pre_header, ' ');
		
		info->type = IS_HTTP_REQUEST;
		if (url && http > url) {
			HTTP_INFO(info).request.method = estrndup(pre_header, url - pre_header);
			HTTP_INFO(info).request.URI = estrndup(url + 1, http - url - 2);
		} else {
			HTTP_INFO(info).request.method = ecalloc(1,1);
			HTTP_INFO(info).request.URI = ecalloc(1,1);
		}
		
		return SUCCESS;
	}
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

