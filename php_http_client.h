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

#ifndef PHP_HTTP_CLIENT_H
#define PHP_HTTP_CLIENT_H

#include "php_http_message_body.h"
#include "php_http_message_parser.h"

typedef struct php_http_client_progress_state {
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
} php_http_client_progress_state_t;

#define PHP_HTTP_CLIENT_PROGRESS_CALLBACK_USER 0
#define PHP_HTTP_CLIENT_PROGRESS_CALLBACK_INTERN 1
typedef struct php_http_client_progress_callback {
	union {
		zval *user;
		void (*intern)(php_http_client_progress_state_t* TSRMLS_DC);
	} func;
	unsigned type:1;
} php_http_client_progress_callback_t;

typedef struct php_http_client_progress {
	php_http_client_progress_state_t state;
	php_http_client_progress_callback_t *callback;
	unsigned in_cb:1;
} php_http_client_progress_t;

static inline void php_http_client_progress_dtor(php_http_client_progress_t *progress TSRMLS_DC)
{
	if (progress->callback) {
		if (progress->callback->type == PHP_HTTP_CLIENT_PROGRESS_CALLBACK_USER) {
			zval_ptr_dtor(&progress->callback->func.user);
		}
		efree(progress->callback);
	}
	memset(progress, 0, sizeof(*progress));
}

static inline void php_http_client_progress_notify(php_http_client_progress_t *progress TSRMLS_DC)
{
	if (progress->callback) {
		zval retval;

		INIT_PZVAL(&retval);
		ZVAL_NULL(&retval);

		with_error_handling(EH_NORMAL, NULL) {
			switch (progress->callback->type) {
				case PHP_HTTP_CLIENT_PROGRESS_CALLBACK_USER:
					progress->in_cb = 1;
					call_user_function(EG(function_table), NULL, progress->callback->func.user, &retval, 0, NULL TSRMLS_CC);
					progress->in_cb = 0;
					break;
				case PHP_HTTP_CLIENT_PROGRESS_CALLBACK_INTERN:
					progress->callback->func.intern(&progress->state TSRMLS_CC);
					break;
				default:
					break;
			}
		} end_error_handling();

		zval_dtor(&retval);
	}
}

typedef enum php_http_client_setopt_opt {
	PHP_HTTP_CLIENT_OPT_SETTINGS,						/* HashTable* */
	PHP_HTTP_CLIENT_OPT_PROGRESS_CALLBACK,				/* php_http_client_progress_callback_t* */
	PHP_HTTP_CLIENT_OPT_COOKIES_ENABLE,					/* - */
	PHP_HTTP_CLIENT_OPT_COOKIES_RESET,					/* - */
	PHP_HTTP_CLIENT_OPT_COOKIES_RESET_SESSION,			/* - */
	PHP_HTTP_CLIENT_OPT_COOKIES_FLUSH,					/* - */
} php_http_client_setopt_opt_t;

typedef enum php_http_client_getopt_opt {
	PHP_HTTP_CLIENT_OPT_PROGRESS_INFO,		/* php_http_client_progress_t** */
	PHP_HTTP_CLIENT_OPT_TRANSFER_INFO,		/* HashTable* */
} php_http_client_getopt_opt_t;

typedef struct php_http_client *(*php_http_client_init_func_t)(struct php_http_client *h, void *arg);
typedef struct php_http_client *(*php_http_client_copy_func_t)(struct php_http_client *from, struct php_http_client *to);
typedef void (*php_http_client_dtor_func_t)(struct php_http_client *h);
typedef STATUS (*php_http_client_exec_func_t)(struct php_http_client *h, php_http_message_t *msg);
typedef STATUS (*php_http_client_reset_func_t)(struct php_http_client *h);
typedef STATUS (*php_http_client_setopt_func_t)(struct php_http_client *h, php_http_client_setopt_opt_t opt, void *arg);
typedef STATUS (*php_http_client_getopt_func_t)(struct php_http_client *h, php_http_client_getopt_opt_t opt, void *arg);

typedef struct php_http_client_ops {
	php_resource_factory_ops_t *rsrc;
	php_http_client_init_func_t init;
	php_http_client_copy_func_t copy;
	php_http_client_dtor_func_t dtor;
	php_http_client_reset_func_t reset;
	php_http_client_exec_func_t exec;
	php_http_client_setopt_func_t setopt;
	php_http_client_getopt_func_t getopt;
	php_http_new_t create_object;
	zend_class_entry *(*class_entry)(void);
} php_http_client_ops_t;

typedef struct php_http_client {
	void *ctx;
	php_resource_factory_t *rf;
	php_http_client_ops_t *ops;
	struct {
		php_http_message_parser_t *parser;
		php_http_message_t *message;
		php_http_buffer_t *buffer;
	} request;
	struct {
		php_http_message_parser_t *parser;
		php_http_message_t *message;
		php_http_buffer_t *buffer;
	} response;
#ifdef ZTS
	void ***ts;
#endif
} php_http_client_t;

PHP_HTTP_API php_http_client_t *php_http_client_init(php_http_client_t *h, php_http_client_ops_t *ops, php_resource_factory_t *rf, void *init_arg TSRMLS_DC);
PHP_HTTP_API php_http_client_t *php_http_client_copy(php_http_client_t *from, php_http_client_t *to);
PHP_HTTP_API STATUS php_http_client_exec(php_http_client_t *h, php_http_message_t *msg);
PHP_HTTP_API STATUS php_http_client_reset(php_http_client_t *h);
PHP_HTTP_API STATUS php_http_client_setopt(php_http_client_t *h, php_http_client_setopt_opt_t opt, void *arg);
PHP_HTTP_API STATUS php_http_client_getopt(php_http_client_t *h, php_http_client_getopt_opt_t opt, void *arg);
PHP_HTTP_API void php_http_client_dtor(php_http_client_t *h);
PHP_HTTP_API void php_http_client_free(php_http_client_t **h);

typedef struct php_http_client_object {
	zend_object zo;
	php_http_client_t *client;
} php_http_client_object_t;

zend_object_value php_http_client_object_new(zend_class_entry *ce TSRMLS_DC);
zend_object_value php_http_client_object_new_ex(zend_class_entry *ce, php_http_client_t *r, php_http_client_object_t **ptr TSRMLS_DC);
zend_object_value php_http_client_object_clone(zval *zobject TSRMLS_DC);
void php_http_client_object_free(void *object TSRMLS_DC);

zend_class_entry *php_http_client_get_class_entry(void);
zend_object_handlers *php_http_client_get_object_handlers(void);

STATUS php_http_client_object_handle_request(zval *zclient, zval **zreq TSRMLS_DC);
STATUS php_http_client_object_handle_response(zval *zclient TSRMLS_DC);

void php_http_client_options_set(zval *this_ptr, zval *opts TSRMLS_DC);
void php_http_client_options_set_subr(zval *this_ptr, char *key, size_t len, zval *opts, int overwrite TSRMLS_DC);
void php_http_client_options_get_subr(zval *this_ptr, char *key, size_t len, zval *return_value TSRMLS_DC);

PHP_METHOD(HttpClient, __construct);
PHP_METHOD(HttpClient, getObservers);
PHP_METHOD(HttpClient, notify);
PHP_METHOD(HttpClient, attach);
PHP_METHOD(HttpClient, detach);
PHP_METHOD(HttpClient, getProgress);
PHP_METHOD(HttpClient, getTransferInfo);
PHP_METHOD(HttpClient, setOptions);
PHP_METHOD(HttpClient, getOptions);
PHP_METHOD(HttpClient, addSslOptions);
PHP_METHOD(HttpClient, setSslOptions);
PHP_METHOD(HttpClient, getSslOptions);
PHP_METHOD(HttpClient, addCookies);
PHP_METHOD(HttpClient, getCookies);
PHP_METHOD(HttpClient, setCookies);
PHP_METHOD(HttpClient, enableCookies);
PHP_METHOD(HttpClient, resetCookies);
PHP_METHOD(HttpClient, flushCookies);
PHP_METHOD(HttpClient, setRequest);
PHP_METHOD(HttpClient, getRequest);
PHP_METHOD(HttpClient, send);
PHP_METHOD(HttpClient, request);
PHP_METHOD(HttpClient, getResponseMessage);
PHP_METHOD(HttpClient, getRequestMessage);
PHP_METHOD(HttpClient, getHistory);
PHP_METHOD(HttpClient, clearHistory);
PHP_METHOD(HttpClient, getResponseMessageClass);
PHP_METHOD(HttpClient, setResponseMessageClass);

PHP_MINIT_FUNCTION(http_client);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

