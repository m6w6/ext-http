/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2011, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http.h"

PHP_HTTP_API void php_http_info_default_callback(void **nothing, HashTable **headers, php_http_info_t *info TSRMLS_DC)
{
	zval array;
	(void) nothing;
	
	INIT_PZVAL_ARRAY(&array, *headers);
	
	switch (info->type) {
		case PHP_HTTP_REQUEST:
			add_assoc_string(&array, "Request Method", PHP_HTTP_INFO(info).request.method, 1);
			add_assoc_string(&array, "Request Url", PHP_HTTP_INFO(info).request.url, 1);
			break;
		
		case PHP_HTTP_RESPONSE:
			add_assoc_long(&array, "Response Code", (long) PHP_HTTP_INFO(info).response.code);
			add_assoc_string(&array, "Response Status", PHP_HTTP_INFO(info).response.status, 1);
			break;

		case PHP_HTTP_NONE:
			break;
	}
}

PHP_HTTP_API php_http_info_t *php_http_info_init(php_http_info_t *i TSRMLS_DC)
{
	if (!i) {
		i = emalloc(sizeof(*i));
	}

	memset(i, 0, sizeof(*i));

	return i;
}

PHP_HTTP_API void php_http_info_dtor(php_http_info_t *i)
{
	switch (i->type) {
		case PHP_HTTP_REQUEST:
			STR_SET(PHP_HTTP_INFO(i).request.method, NULL);
			STR_SET(PHP_HTTP_INFO(i).request.url, NULL);
			break;
		
		case PHP_HTTP_RESPONSE:
			STR_SET(PHP_HTTP_INFO(i).response.status, NULL);
			break;
		
		default:
			break;
	}
}

PHP_HTTP_API void php_http_info_free(php_http_info_t **i)
{
	if (*i) {
		php_http_info_dtor(*i);
		efree(*i);
		*i = NULL;
	}
}

PHP_HTTP_API php_http_info_t *php_http_info_parse(php_http_info_t *info, const char *pre_header TSRMLS_DC)
{
	const char *end, *http;
	zend_bool free_info = !info;
	
	/* sane parameter */
	if ((!pre_header) || (!*pre_header)) {
		return NULL;
	}
	
	/* where's the end of the line */
	if (!(end = php_http_locate_eol(pre_header, NULL))) {
		end = pre_header + strlen(pre_header);
	}
	
	/* there must be HTTP/1.x in the line */
	if (!(http = php_http_locate_str(pre_header, end - pre_header, "HTTP/1.", lenof("HTTP/1.")))) {
		return NULL;
	}
	
	info = php_http_info_init(info TSRMLS_CC);

	/* and nothing than SPACE or NUL after HTTP/1.x */
	if (!php_http_version_parse(&info->http.version, http TSRMLS_CC)
	||	(http[lenof("HTTP/1.1")] && (!PHP_HTTP_IS_CTYPE(space, http[lenof("HTTP/1.1")])))) {
		if (free_info) {
			php_http_info_free(&info);
		}
		return NULL;
	}

#if 0
	{
		char *line = estrndup(pre_header, end - pre_header);
		fprintf(stderr, "http_parse_info('%s')\n", line);
		efree(line);
	}
#endif

	/* is response */
	if (pre_header == http) {
		char *status = NULL;
		const char *code = http + sizeof("HTTP/1.1");
		
		info->type = PHP_HTTP_RESPONSE;
		while (' ' == *code) ++code;
		if (code && end > code) {
			PHP_HTTP_INFO(info).response.code = strtol(code, &status, 10);
		} else {
			PHP_HTTP_INFO(info).response.code = 0;
		}
		if (status && end > status) {
			while (' ' == *status) ++status;
			PHP_HTTP_INFO(info).response.status = estrndup(status, end - status);
		} else {
			PHP_HTTP_INFO(info).response.status = NULL;
		}
		
		return info;
	}
	
	/* is request */
	else if (!http[lenof("HTTP/1.x")] || http[lenof("HTTP/1.x")] == '\r' || http[lenof("HTTP/1.x")] == '\n') {
		const char *url = strchr(pre_header, ' ');
		
		info->type = PHP_HTTP_REQUEST;
		if (url && http > url) {
			PHP_HTTP_INFO(info).request.method = estrndup(pre_header, url - pre_header);
			while (' ' == *url) ++url;
			while (' ' == *(http-1)) --http;
			if (http > url) {
				PHP_HTTP_INFO(info).request.url = estrndup(url, http - url);
			} else {
				efree(PHP_HTTP_INFO(info).request.method);
				return NULL;
			}
		} else {
			PHP_HTTP_INFO(info).request.method = NULL;
			PHP_HTTP_INFO(info).request.url = NULL;
		}
		
		return info;
	}

	/* some darn header containing HTTP/1.x */
	else {
		if (free_info) {
			php_http_info_free(&info);
		}
		return NULL;
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

