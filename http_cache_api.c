/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2006, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_SAPI
#include "php_http.h"

#include "php_output.h"
#include "php_streams.h"

#include "php_http_api.h"
#include "php_http_cache_api.h"
#include "php_http_date_api.h"
#include "php_http_send_api.h"

/* {{{ char *http_etag(void *, size_t, http_send_mode) */
PHP_HTTP_API char *_http_etag(const void *data_ptr, size_t data_len, http_send_mode data_mode TSRMLS_DC)
{
	void *ctx = http_etag_init();
	
	if (data_mode == SEND_DATA) {
		http_etag_update(ctx, data_ptr, data_len);
	} else {
		STATUS ss = FAILURE;
		php_stream_statbuf ssb;
		
		if (data_mode == SEND_RSRC) {
			ss = php_stream_stat((php_stream *) data_ptr, &ssb);
		} else {
			ss = php_stream_stat_path((char *) data_ptr, &ssb);
		}
		
		if (SUCCESS != ss) {
			efree(ctx);
			return NULL;
		} else {
			size_t ssb_len;
			char ssb_buf[128];
			
			ssb_len = snprintf(ssb_buf, sizeof(ssb_buf), "%ld=%ld=%ld", (long) ssb.sb.st_mtime, 
															(long) ssb.sb.st_ino, 
															(long) ssb.sb.st_size);
			http_etag_update(ctx, ssb_buf, ssb_len);
		}
	}
	
	return http_etag_finish(ctx);
}
/* }}} */

/* {{{ time_t http_last_modified(void *, http_send_mode) */
PHP_HTTP_API time_t _http_last_modified(const void *data_ptr, http_send_mode data_mode TSRMLS_DC)
{
	php_stream_statbuf ssb;

	switch (data_mode) {
		case SEND_DATA:	return HTTP_G->request.time;
		case SEND_RSRC:	return php_stream_stat((php_stream *) data_ptr, &ssb) ? 0 : ssb.sb.st_mtime;
		default:		return php_stream_stat_path((char *) data_ptr, &ssb) ? 0 : ssb.sb.st_mtime;
	}
}
/* }}} */

/* {{{ zend_bool http_match_last_modified(char *, time_t) */
PHP_HTTP_API zend_bool _http_match_last_modified_ex(const char *entry, time_t t, zend_bool enforce_presence TSRMLS_DC)
{
	zend_bool retval;
	zval *zmodified;
	char *modified, *chr_ptr;

	if (!(zmodified = http_get_server_var(entry, 1))) {
		return !enforce_presence;
	}

	modified = estrndup(Z_STRVAL_P(zmodified), Z_STRLEN_P(zmodified));
	if ((chr_ptr = strrchr(modified, ';'))) {
		chr_ptr = 0;
	}
	
	retval = (t <= http_parse_date_ex(modified, 1));
	efree(modified);
	return retval;
}
/* }}} */

/* {{{ zend_bool http_match_etag(char *, char *) */
PHP_HTTP_API zend_bool _http_match_etag_ex(const char *entry, const char *etag, zend_bool enforce_presence TSRMLS_DC)
{
	zval *zetag;
	char *quoted_etag;
	zend_bool result;

	if (!(zetag = http_get_server_var_ex(entry, strlen(entry)+1, 1))) {
		return !enforce_presence;
	}

	if (NULL != strchr(Z_STRVAL_P(zetag), '*')) {
		return 1;
	}

	spprintf(&quoted_etag, 0, "\"%s\"", etag);
	if (!strchr(Z_STRVAL_P(zetag), ',')) {
		result = !strcmp(Z_STRVAL_P(zetag), quoted_etag);
	} else {
		result = (NULL != strstr(Z_STRVAL_P(zetag), quoted_etag));
	}
	efree(quoted_etag);
	
	return result;
}
/* }}} */

/* {{{ STATUS http_cache_last_modified(time_t, time_t, char *, size_t) */
PHP_HTTP_API STATUS _http_cache_last_modified(time_t last_modified,
	time_t send_modified, const char *cache_control, size_t cc_len TSRMLS_DC)
{
	char *sent_header = NULL;
	
	if (SG(headers_sent)) {
		return FAILURE;
	}
	
	if (cc_len && (SUCCESS != http_send_cache_control(cache_control, cc_len))) {
		return FAILURE;
	}

	if (SUCCESS != http_send_last_modified_ex(send_modified, &sent_header)) {
		return FAILURE;
	}

	if (http_match_last_modified("HTTP_IF_MODIFIED_SINCE", last_modified)) {
		http_exit_ex(304, sent_header, NULL, 0);
	} else {
		STR_FREE(sent_header);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_cache_etag(char *, size_t, char *, size_t) */
PHP_HTTP_API STATUS _http_cache_etag(const char *etag, size_t etag_len,
	const char *cache_control, size_t cc_len TSRMLS_DC)
{
	char *sent_header = NULL;
	
	if (SG(headers_sent)) {
		return FAILURE;
	}
	
	if (cc_len && (SUCCESS != http_send_cache_control(cache_control, cc_len))) {
		return FAILURE;
	}

	if (etag_len) {
		if (SUCCESS != http_send_etag_ex(etag, etag_len, &sent_header)) {
			return FAILURE;
		}
		if (http_match_etag("HTTP_IF_NONE_MATCH", etag)) {
			http_exit_ex(304, sent_header, NULL, 0);
		} else {
			STR_FREE(sent_header);
		}
		return SUCCESS;
	}
	
	/* start ob_etaghandler */
	return http_start_ob_etaghandler();
}
/* }}} */

PHP_HTTP_API STATUS _http_start_ob_etaghandler(TSRMLS_D)
{
	/* already running? */
	if (php_ob_handler_used("ob_etaghandler" TSRMLS_CC)) {
		http_error(HE_WARNING, HTTP_E_RUNTIME, "ob_etaghandler can only be used once");
		return FAILURE;
	}
	
	HTTP_G->etag.started = 1;
	return php_start_ob_buffer_named("ob_etaghandler", HTTP_G->send.buffer_size, 0 TSRMLS_CC);
}

PHP_HTTP_API zend_bool _http_interrupt_ob_etaghandler(TSRMLS_D)
{
	if (HTTP_G->etag.started) {
		HTTP_G->etag.started = 0;
		if (HTTP_G->etag.ctx) {
			efree(HTTP_G->etag.ctx);
			HTTP_G->etag.ctx = NULL;
		}
		return 1;
	}
	return 0;
}

/* {{{ void http_ob_etaghandler(char *, uint, char **, uint *, int) */
void _http_ob_etaghandler(char *output, uint output_len,
	char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
	/* passthru */
	*handled_output_len = output_len;
	*handled_output = estrndup(output, output_len);
	
	/* are we supposed to run? */
	if (HTTP_G->etag.started) {
		/* initialize the etag context */
		if (mode & PHP_OUTPUT_HANDLER_START) {
			HTTP_G->etag.ctx = http_etag_init();
		}
		
		/* update */
		http_etag_update(HTTP_G->etag.ctx, output, output_len);
		
		/* finish */
		if (mode & PHP_OUTPUT_HANDLER_END) {
			char *sent_header = NULL;
			char *etag = http_etag_finish(HTTP_G->etag.ctx);
			
			HTTP_G->etag.ctx = NULL;
			
			http_send_cache_control(HTTP_DEFAULT_CACHECONTROL, lenof(HTTP_DEFAULT_CACHECONTROL));
			http_send_etag_ex(etag, strlen(etag), &sent_header);
			
			if (http_match_etag("HTTP_IF_NONE_MATCH", etag)) {
				/* force exit; ob within ob does not work */
				HTTP_G->force_exit = 1;
				http_exit_ex(304, sent_header, etag, 0);
			}
			
			STR_FREE(sent_header);
			STR_FREE(etag);
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

