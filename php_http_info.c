/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2014, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#include "php_http_api.h"

php_http_info_t *php_http_info_init(php_http_info_t *i)
{
	if (!i) {
		i = emalloc(sizeof(*i));
	}

	memset(i, 0, sizeof(*i));

	return i;
}

void php_http_info_dtor(php_http_info_t *i)
{
	switch (i->type) {
		case PHP_HTTP_REQUEST:
			PTR_SET(PHP_HTTP_INFO(i).request.method, NULL);
			PTR_SET(PHP_HTTP_INFO(i).request.url, NULL);
			break;
		
		case PHP_HTTP_RESPONSE:
			PTR_SET(PHP_HTTP_INFO(i).response.status, NULL);
			break;
		
		default:
			break;
	}
}

void php_http_info_free(php_http_info_t **i)
{
	if (*i) {
		php_http_info_dtor(*i);
		efree(*i);
		*i = NULL;
	}
}

php_http_info_t *php_http_info_parse(php_http_info_t *info, const char *pre_header)
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
	if (!(http = php_http_locate_str(pre_header, end - pre_header, "HTTP/", lenof("HTTP/")))) {
		return NULL;
	}
	
	info = php_http_info_init(info);

	/* and nothing than SPACE or NUL after HTTP/X.x */
	if (!php_http_version_parse(&info->http.version, http)
	||	(http[lenof("HTTP/X.x")] && (!PHP_HTTP_IS_CTYPE(space, http[lenof("HTTP/X.x")])))) {
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
		const char *status = NULL, *code = http + sizeof("HTTP/X.x");
		
		info->type = PHP_HTTP_RESPONSE;
		while (code < end && ' ' == *code) ++code;
		if (code && end > code) {
			/* rfc7230#3.1.2 The status-code element is a 3-digit integer code */
			PHP_HTTP_INFO(info).response.code = 100*(*code++ - '0');
			PHP_HTTP_INFO(info).response.code += 10*(*code++ - '0');
			PHP_HTTP_INFO(info).response.code +=     *code++ - '0';
			if (PHP_HTTP_INFO(info).response.code < 100 || PHP_HTTP_INFO(info).response.code > 599) {
				if (free_info) {
					php_http_info_free(&info);
				}
				return NULL;
			}
			status = code;
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
	else if (*(http - 1) == ' ' && (!http[lenof("HTTP/X.x")] || http[lenof("HTTP/X.x")] == '\r' || http[lenof("HTTP/X.x")] == '\n')) {
		const char *url = strchr(pre_header, ' ');
		
		info->type = PHP_HTTP_REQUEST;
		if (url && http > url) {
			size_t url_len = url - pre_header;

			PHP_HTTP_INFO(info).request.method = estrndup(pre_header, url_len);

			while (' ' == *url) ++url;
			while (' ' == *(http-1)) --http;

			if (http > url) {
				/* CONNECT presents an authority only */
				if (strcasecmp(PHP_HTTP_INFO(info).request.method, "CONNECT")) {
					PHP_HTTP_INFO(info).request.url = php_http_url_parse(url, http - url, ~0);
				} else {
					PHP_HTTP_INFO(info).request.url = php_http_url_parse_authority(url, http - url, ~0);
				}
				if (!PHP_HTTP_INFO(info).request.url) {
					PTR_SET(PHP_HTTP_INFO(info).request.method, NULL);
					return NULL;
				}
			} else {
				PTR_SET(PHP_HTTP_INFO(info).request.method, NULL);
				return NULL;
			}
		} else {
			PHP_HTTP_INFO(info).request.method = NULL;
			PHP_HTTP_INFO(info).request.url = NULL;
		}
		
		return info;
	}

	/* some darn header containing HTTP/X.x */
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

