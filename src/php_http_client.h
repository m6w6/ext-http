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

#ifndef PHP_HTTP_CLIENT_H
#define PHP_HTTP_CLIENT_H

typedef enum php_http_client_setopt_opt {
	PHP_HTTP_CLIENT_OPT_ENABLE_PIPELINING,
	PHP_HTTP_CLIENT_OPT_USE_EVENTS,
	PHP_HTTP_CLIENT_OPT_CONFIGURATION,
} php_http_client_setopt_opt_t;

typedef enum php_http_client_getopt_opt {
	PHP_HTTP_CLIENT_OPT_PROGRESS_INFO,		/* php_http_client_enqueue_t*, php_http_client_progress_state_t** */
	PHP_HTTP_CLIENT_OPT_TRANSFER_INFO,		/* php_http_client_enqueue_t*, HashTable* */
	PHP_HTTP_CLIENT_OPT_AVAILABLE_OPTIONS,		/* NULL, HashTable* */
	PHP_HTTP_CLIENT_OPT_AVAILABLE_CONFIGURATION,/* NULL, HashTable */
} php_http_client_getopt_opt_t;

typedef struct php_http_client_enqueue {
	php_http_message_t *request; /* unique */
	HashTable *options;
	void (*dtor)(struct php_http_client_enqueue *);
	void *opaque;
	struct {
		zend_fcall_info fci;
		zend_fcall_info_cache fcc;
	} closure;
} php_http_client_enqueue_t;

typedef struct php_http_client *(*php_http_client_init_func_t)(struct php_http_client *p, void *init_arg);
typedef struct php_http_client *(*php_http_client_copy_func_t)(struct php_http_client *from, struct php_http_client *to);
typedef void (*php_http_client_dtor_func_t)(struct php_http_client *p);
typedef void (*php_http_client_reset_func_t)(struct php_http_client *p);
typedef ZEND_RESULT_CODE (*php_http_client_exec_func_t)(struct php_http_client *p);
typedef int (*php_http_client_once_func_t)(struct php_http_client *p);
typedef ZEND_RESULT_CODE (*php_http_client_wait_func_t)(struct php_http_client *p, struct timeval *custom_timeout);
typedef ZEND_RESULT_CODE (*php_http_client_enqueue_func_t)(struct php_http_client *p, php_http_client_enqueue_t *enqueue);
typedef ZEND_RESULT_CODE (*php_http_client_dequeue_func_t)(struct php_http_client *p, php_http_client_enqueue_t *enqueue);
typedef ZEND_RESULT_CODE (*php_http_client_setopt_func_t)(struct php_http_client *p, php_http_client_setopt_opt_t opt, void *arg);
typedef ZEND_RESULT_CODE (*php_http_client_getopt_func_t)(struct php_http_client *h, php_http_client_getopt_opt_t opt, void *arg, void **res);

typedef struct php_http_client_ops {
	php_resource_factory_ops_t *rsrc;
	php_http_client_init_func_t init;
	php_http_client_copy_func_t copy;
	php_http_client_dtor_func_t dtor;
	php_http_client_reset_func_t reset;
	php_http_client_exec_func_t exec;
	php_http_client_wait_func_t wait;
	php_http_client_once_func_t once;
	php_http_client_enqueue_func_t enqueue;
	php_http_client_dequeue_func_t dequeue;
	php_http_client_setopt_func_t setopt;
	php_http_client_getopt_func_t getopt;
} php_http_client_ops_t;

typedef struct php_http_client_driver {
	zend_string *driver_name;
	zend_string *client_name;
	zend_string *request_name;
	php_http_client_ops_t *client_ops;
} php_http_client_driver_t;

PHP_HTTP_API ZEND_RESULT_CODE php_http_client_driver_add(php_http_client_driver_t *driver);
PHP_HTTP_API php_http_client_driver_t *php_http_client_driver_get(zend_string *name);

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

typedef ZEND_RESULT_CODE (*php_http_client_response_callback_t)(void *arg, struct php_http_client *client, php_http_client_enqueue_t *e, php_http_message_t **response);
typedef void (*php_http_client_progress_callback_t)(void *arg, struct php_http_client *client, php_http_client_enqueue_t *e, php_http_client_progress_state_t *state);
typedef void (*php_http_client_debug_callback_t)(void *arg, struct php_http_client *client, php_http_client_enqueue_t *e, unsigned type, const char *data, size_t size);

#define PHP_HTTP_CLIENT_DEBUG_INFO		0x00
#define PHP_HTTP_CLIENT_DEBUG_IN		0x01
#define PHP_HTTP_CLIENT_DEBUG_OUT		0x02
#define PHP_HTTP_CLIENT_DEBUG_HEADER	0x10
#define PHP_HTTP_CLIENT_DEBUG_BODY		0x20
#define PHP_HTTP_CLIENT_DEBUG_SSL		0x40

typedef struct php_http_client {
	void *ctx;
	php_resource_factory_t *rf;
	php_http_client_ops_t *ops;

	struct {
		struct {
			php_http_client_response_callback_t func;
			void *arg;
		} response;
		struct {
			php_http_client_progress_callback_t func;
			void *arg;
		} progress;
		struct {
			php_http_client_debug_callback_t func;
			void *arg;
		} debug;
		unsigned depth;
	} callback;

	zend_llist requests;
	zend_llist responses;
} php_http_client_t;

PHP_HTTP_API zend_class_entry *php_http_client_get_class_entry();

typedef struct php_http_client_object {
	php_http_client_t *client;
	php_http_object_method_t *update;
	php_http_object_method_t notify;
	struct {
		zend_fcall_info fci;
		zend_fcall_info_cache fcc;
	} debug;
	long iterator;
	zval *gc;
	zend_object zo;
} php_http_client_object_t;

PHP_HTTP_API php_http_client_t *php_http_client_init(php_http_client_t *h, php_http_client_ops_t *ops, php_resource_factory_t *rf, void *init_arg);
PHP_HTTP_API php_http_client_t *php_http_client_copy(php_http_client_t *from, php_http_client_t *to);
PHP_HTTP_API void php_http_client_dtor(php_http_client_t *h);
PHP_HTTP_API void php_http_client_free(php_http_client_t **h);

PHP_HTTP_API ZEND_RESULT_CODE php_http_client_enqueue(php_http_client_t *h, php_http_client_enqueue_t *enqueue);
PHP_HTTP_API ZEND_RESULT_CODE php_http_client_dequeue(php_http_client_t *h, php_http_message_t *request);

PHP_HTTP_API ZEND_RESULT_CODE php_http_client_wait(php_http_client_t *h, struct timeval *custom_timeout);
PHP_HTTP_API int php_http_client_once(php_http_client_t *h);

PHP_HTTP_API ZEND_RESULT_CODE php_http_client_exec(php_http_client_t *h);
PHP_HTTP_API void php_http_client_reset(php_http_client_t *h);

PHP_HTTP_API ZEND_RESULT_CODE php_http_client_setopt(php_http_client_t *h, php_http_client_setopt_opt_t opt, void *arg);
PHP_HTTP_API ZEND_RESULT_CODE php_http_client_getopt(php_http_client_t *h, php_http_client_getopt_opt_t opt, void *arg, void *res_ptr);

typedef int (*php_http_client_enqueue_cmp_func_t)(php_http_client_enqueue_t *cmp, void *arg);
/* compare with request message pointer if compare_func is NULL */
PHP_HTTP_API php_http_client_enqueue_t *php_http_client_enqueued(php_http_client_t *h, void *compare_arg, php_http_client_enqueue_cmp_func_t compare_func);

PHP_MINIT_FUNCTION(http_client);
PHP_MSHUTDOWN_FUNCTION(http_client);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
