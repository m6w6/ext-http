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

#ifndef PHP_HTTP_REQUEST_H
#define PHP_HTTP_REQUEST_H

#include "php_http_message_body.h"
#include "php_http_message_parser.h"

typedef struct php_http_request_progress_state {
	struct {
		double now;
		double total;
	} ul;
	struct {
		double now;
		double total;
	} dl;
	const char *info;
	unsigned started:1;
	unsigned finished:1;
} php_http_request_progress_state_t;

#define PHP_HTTP_REQUEST_PROGRESS_CALLBACK_USER 0
#define PHP_HTTP_REQUEST_PROGRESS_CALLBACK_INTERN 1
typedef struct php_http_request_progress_callback {
	union {
		zval *user;
		void (*intern)(php_http_request_progress_state_t* TSRMLS_DC);
	} func;
	unsigned type:1;
	unsigned pass_state:1;
} php_http_request_progress_callback_t;

typedef struct php_http_request_progress {
	php_http_request_progress_state_t state;
	php_http_request_progress_callback_t *callback;
	unsigned in_cb:1;
} php_http_request_progress_t;

static inline void php_http_request_progress_dtor(php_http_request_progress_t *progress TSRMLS_DC)
{
	if (progress->callback) {
		if (progress->callback->type == PHP_HTTP_REQUEST_PROGRESS_CALLBACK_USER) {
			zval_ptr_dtor(&progress->callback->func.user);
		}
		efree(progress->callback);
	}
	memset(progress, 0, sizeof(*progress));
}

static inline void php_http_request_progress_notify(php_http_request_progress_t *progress TSRMLS_DC)
{
	if (progress->callback) {
		zval retval;

		INIT_PZVAL(&retval);
		ZVAL_NULL(&retval);

		with_error_handling(EH_NORMAL, NULL) {
			switch (progress->callback->type) {
				case PHP_HTTP_REQUEST_PROGRESS_CALLBACK_USER:
					if (progress->callback->pass_state) {
						zval *param;

						MAKE_STD_ZVAL(param);
						array_init(param);
						add_assoc_bool(param, "started", progress->state.started);
						add_assoc_bool(param, "finished", progress->state.finished);
						add_assoc_string(param, "info", estrdup(progress->state.info), 0);
						add_assoc_double(param, "dltotal", progress->state.dl.total);
						add_assoc_double(param, "dlnow", progress->state.dl.now);
						add_assoc_double(param, "ultotal", progress->state.ul.total);
						add_assoc_double(param, "ulnow", progress->state.ul.now);

						progress->in_cb = 1;
						call_user_function(EG(function_table), NULL, progress->callback->func.user, &retval, 1, &param TSRMLS_CC);
						progress->in_cb = 0;

						zval_ptr_dtor(&param);
					} else {
						progress->in_cb = 1;
						call_user_function(EG(function_table), NULL, progress->callback->func.user, &retval, 0, NULL TSRMLS_CC);
						progress->in_cb = 0;
					}
					break;
				case PHP_HTTP_REQUEST_PROGRESS_CALLBACK_INTERN:
					progress->callback->func.intern(progress->callback->pass_state ? &progress->state : NULL TSRMLS_CC);
					break;
				default:
					break;
			}
		} end_error_handling();

		zval_dtor(&retval);
	}
}

typedef enum php_http_request_setopt_opt {
	PHP_HTTP_REQUEST_OPT_SETTINGS,						/* HashTable* */
	PHP_HTTP_REQUEST_OPT_PROGRESS_CALLBACK,				/* php_http_request_progress_callback_t* */
	PHP_HTTP_REQUEST_OPT_COOKIES_ENABLE,				/* - */
	PHP_HTTP_REQUEST_OPT_COOKIES_RESET,					/* - */
	PHP_HTTP_REQUEST_OPT_COOKIES_RESET_SESSION,			/* - */
	PHP_HTTP_REQUEST_OPT_COOKIES_FLUSH,					/* - */
} php_http_request_setopt_opt_t;

typedef enum php_http_request_getopt_opt {
	PHP_HTTP_REQUEST_OPT_PROGRESS_INFO,		/* php_http_request_progress_t** */
	PHP_HTTP_REQUEST_OPT_TRANSFER_INFO,		/* HashTable* */
} php_http_request_getopt_opt_t;

typedef struct php_http_request php_http_request_t;

typedef php_http_request_t *(*php_http_request_init_func_t)(php_http_request_t *h, void *arg);
typedef php_http_request_t *(*php_http_request_copy_func_t)(php_http_request_t *from, php_http_request_t *to);
typedef void (*php_http_request_dtor_func_t)(php_http_request_t *h);
typedef STATUS (*php_http_request_exec_func_t)(php_http_request_t *h, const char *meth, const char *url, php_http_message_body_t *body);
typedef STATUS (*php_http_request_reset_func_t)(php_http_request_t *h);
typedef STATUS (*php_http_request_setopt_func_t)(php_http_request_t *h, php_http_request_setopt_opt_t opt, void *arg);
typedef STATUS (*php_http_request_getopt_func_t)(php_http_request_t *h, php_http_request_getopt_opt_t opt, void *arg);

typedef struct php_http_request_ops {
	php_http_resource_factory_ops_t *rsrc;
	php_http_request_init_func_t init;
	php_http_request_copy_func_t copy;
	php_http_request_dtor_func_t dtor;
	php_http_request_reset_func_t reset;
	php_http_request_exec_func_t exec;
	php_http_request_setopt_func_t setopt;
	php_http_request_getopt_func_t getopt;
} php_http_request_ops_t;

PHP_HTTP_API php_http_request_ops_t *php_http_request_get_default_ops(TSRMLS_D);

struct php_http_request {
	void *ctx;
	php_http_resource_factory_t *rf;
	php_http_request_ops_t *ops;
	php_http_message_parser_t *parser;
	php_http_message_t *message;
	php_http_buffer_t *buffer;
#ifdef ZTS
	void ***ts;
#endif
};

PHP_HTTP_API php_http_request_t *php_http_request_init(php_http_request_t *h, php_http_request_ops_t *ops, php_http_resource_factory_t *rf, void *init_arg TSRMLS_DC);
PHP_HTTP_API php_http_request_t *php_http_request_copy(php_http_request_t *from, php_http_request_t *to);
PHP_HTTP_API STATUS php_http_request_exec(php_http_request_t *h, const char *meth, const char *url, php_http_message_body_t *body);
PHP_HTTP_API STATUS php_http_request_reset(php_http_request_t *h);
PHP_HTTP_API STATUS php_http_request_setopt(php_http_request_t *h, php_http_request_setopt_opt_t opt, void *arg);
PHP_HTTP_API STATUS php_http_request_getopt(php_http_request_t *h, php_http_request_getopt_opt_t opt, void *arg);
PHP_HTTP_API void php_http_request_dtor(php_http_request_t *h);
PHP_HTTP_API void php_http_request_free(php_http_request_t **h);

typedef struct php_http_request_object {
	zend_object zo;
	php_http_request_t *request;
} php_http_request_object_t;

extern zend_class_entry *php_http_request_class_entry;
extern zend_function_entry php_http_request_method_entry[];

extern zend_object_value php_http_request_object_new(zend_class_entry *ce TSRMLS_DC);
extern zend_object_value php_http_request_object_new_ex(zend_class_entry *ce, php_http_request_t *r, php_http_request_object_t **ptr TSRMLS_DC);
extern zend_object_value php_http_request_object_clone(zval *zobject TSRMLS_DC);
extern void php_http_request_object_free(void *object TSRMLS_DC);

extern STATUS php_http_request_object_requesthandler(php_http_request_object_t *obj, zval *this_ptr, char **meth, char **url, php_http_message_body_t **body TSRMLS_DC);
extern STATUS php_http_request_object_responsehandler(php_http_request_object_t *obj, zval *this_ptr TSRMLS_DC);

PHP_METHOD(HttpRequest, __construct);
PHP_METHOD(HttpRequest, getObservers);
PHP_METHOD(HttpRequest, notify);
PHP_METHOD(HttpRequest, attach);
PHP_METHOD(HttpRequest, detach);
PHP_METHOD(HttpRequest, getProgress);
PHP_METHOD(HttpRequest, getTransferInfo);
PHP_METHOD(HttpRequest, setOptions);
PHP_METHOD(HttpRequest, getOptions);
PHP_METHOD(HttpRequest, addSslOptions);
PHP_METHOD(HttpRequest, setSslOptions);
PHP_METHOD(HttpRequest, getSslOptions);
PHP_METHOD(HttpRequest, addHeaders);
PHP_METHOD(HttpRequest, getHeaders);
PHP_METHOD(HttpRequest, setHeaders);
PHP_METHOD(HttpRequest, addCookies);
PHP_METHOD(HttpRequest, getCookies);
PHP_METHOD(HttpRequest, setCookies);
PHP_METHOD(HttpRequest, enableCookies);
PHP_METHOD(HttpRequest, resetCookies);
PHP_METHOD(HttpRequest, flushCookies);
PHP_METHOD(HttpRequest, setMethod);
PHP_METHOD(HttpRequest, getMethod);
PHP_METHOD(HttpRequest, setUrl);
PHP_METHOD(HttpRequest, getUrl);
PHP_METHOD(HttpRequest, setContentType);
PHP_METHOD(HttpRequest, getContentType);
PHP_METHOD(HttpRequest, setQueryData);
PHP_METHOD(HttpRequest, getQueryData);
PHP_METHOD(HttpRequest, addQueryData);
PHP_METHOD(HttpRequest, getBody);
PHP_METHOD(HttpRequest, setBody);
PHP_METHOD(HttpRequest, addBody);
PHP_METHOD(HttpRequest, send);
PHP_METHOD(HttpRequest, getResponseData);
PHP_METHOD(HttpRequest, getResponseHeader);
PHP_METHOD(HttpRequest, getResponseCookies);
PHP_METHOD(HttpRequest, getResponseCode);
PHP_METHOD(HttpRequest, getResponseStatus);
PHP_METHOD(HttpRequest, getResponseBody);
PHP_METHOD(HttpRequest, getResponseMessage);
PHP_METHOD(HttpRequest, getRawResponseMessage);
PHP_METHOD(HttpRequest, getRequestMessage);
PHP_METHOD(HttpRequest, getRawRequestMessage);
PHP_METHOD(HttpRequest, getHistory);
PHP_METHOD(HttpRequest, clearHistory);
PHP_METHOD(HttpRequest, getMessageClass);
PHP_METHOD(HttpRequest, setMessageClass);

extern PHP_MINIT_FUNCTION(http_request);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

