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

#include "php_http_api.h"

#include <ext/spl/spl_observer.h>

PHP_HTTP_API php_http_request_t *php_http_request_init(php_http_request_t *h, php_http_request_ops_t *ops, php_http_resource_factory_t *rf, void *init_arg TSRMLS_DC)
{
	php_http_request_t *free_h = NULL;

	if (!h) {
		free_h = h = emalloc(sizeof(*h));
	}
	memset(h, 0, sizeof(*h));

	h->ops = ops;
	h->rf = rf ? rf : php_http_resource_factory_init(NULL, h->ops->rsrc, h, NULL);
	h->buffer = php_http_buffer_init(NULL);
	h->parser = php_http_message_parser_init(NULL TSRMLS_CC);
	h->message = php_http_message_init(NULL, 0 TSRMLS_CC);

	TSRMLS_SET_CTX(h->ts);

	if (h->ops->init) {
		if (!(h = h->ops->init(h, init_arg))) {
			php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Could not initialize request");
			if (free_h) {
				h->ops->dtor = NULL;
				php_http_request_free(&free_h);
			}
		}
	}

	return h;
}

PHP_HTTP_API void php_http_request_dtor(php_http_request_t *h)
{
	if (h->ops->dtor) {
		h->ops->dtor(h);
	}

	php_http_resource_factory_free(&h->rf);

	php_http_message_parser_free(&h->parser);
	php_http_message_free(&h->message);
	php_http_buffer_free(&h->buffer);
}

PHP_HTTP_API void php_http_request_free(php_http_request_t **h)
{
	if (*h) {
		php_http_request_dtor(*h);
		efree(*h);
		*h = NULL;
	}
}

PHP_HTTP_API php_http_request_t *php_http_request_copy(php_http_request_t *from, php_http_request_t *to)
{
	if (!from->ops->copy) {
		return NULL;
	} else {
		TSRMLS_FETCH_FROM_CTX(from->ts);

		if (!to) {
			to = ecalloc(1, sizeof(*to));
		}

		to->ops = from->ops;
		if (from->rf) {
			php_http_resource_factory_addref(from->rf);
			to->rf = from->rf;
		} else {
			to->rf = php_http_resource_factory_init(NULL, to->ops->rsrc, to, NULL);
		}
		to->buffer = php_http_buffer_init(NULL);
		to->parser = php_http_message_parser_init(NULL TSRMLS_CC);
		to->message = php_http_message_init(NULL, 0 TSRMLS_CC);

		TSRMLS_SET_CTX(to->ts);

		return to->ops->copy(from, to);
	}
}

PHP_HTTP_API STATUS php_http_request_exec(php_http_request_t *h, const char *meth, const char *url, php_http_message_body_t *body)
{
	if (h->ops->exec) {
		return h->ops->exec(h, meth, url, body);
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_request_reset(php_http_request_t *h)
{
	if (h->ops->reset) {
		return h->ops->reset(h);
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_request_setopt(php_http_request_t *h, php_http_request_setopt_opt_t opt, void *arg)
{
	if (h->ops->setopt) {
		return h->ops->setopt(h, opt, arg);
	}
	return FAILURE;
}

PHP_HTTP_API STATUS php_http_request_getopt(php_http_request_t *h, php_http_request_getopt_opt_t opt, void *arg)
{
	if (h->ops->getopt) {
		return h->ops->getopt(h, opt, arg);
	}
	return FAILURE;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpRequest, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpRequest, method, 0)
#define PHP_HTTP_REQUEST_ME(method, visibility)	PHP_ME(HttpRequest, method, PHP_HTTP_ARGS(HttpRequest, method), visibility)
#define PHP_HTTP_REQUEST_ALIAS(method, func)	PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpRequest, method))
#define PHP_HTTP_REQUEST_MALIAS(me, al, vis)	ZEND_FENTRY(me, ZEND_MN(HttpRequest_##al), PHP_HTTP_ARGS(HttpRequest, al), vis)

PHP_HTTP_EMPTY_ARGS(__construct);

PHP_HTTP_EMPTY_ARGS(getOptions);
PHP_HTTP_BEGIN_ARGS(setOptions, 0)
	PHP_HTTP_ARG_VAL(options, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getSslOptions);
PHP_HTTP_BEGIN_ARGS(setSslOptions, 0)
	PHP_HTTP_ARG_VAL(ssl_options, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addSslOptions, 0)
	PHP_HTTP_ARG_VAL(ssl_optins, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getHeaders);
PHP_HTTP_BEGIN_ARGS(setHeaders, 0)
	PHP_HTTP_ARG_VAL(headers, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addHeaders, 1)
	PHP_HTTP_ARG_VAL(headers, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getCookies);
PHP_HTTP_BEGIN_ARGS(setCookies, 0)
	PHP_HTTP_ARG_VAL(cookies, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addCookies, 1)
	PHP_HTTP_ARG_VAL(cookies, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(enableCookies);
PHP_HTTP_BEGIN_ARGS(resetCookies, 0)
	PHP_HTTP_ARG_VAL(session_only, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_EMPTY_ARGS(flushCookies);

PHP_HTTP_EMPTY_ARGS(getUrl);
PHP_HTTP_BEGIN_ARGS(setUrl, 1)
	PHP_HTTP_ARG_VAL(url, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getMethod);
PHP_HTTP_BEGIN_ARGS(setMethod, 1)
	PHP_HTTP_ARG_VAL(request_method, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getContentType);
PHP_HTTP_BEGIN_ARGS(setContentType, 1)
	PHP_HTTP_ARG_VAL(content_type, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getQueryData);
PHP_HTTP_BEGIN_ARGS(setQueryData, 0)
	PHP_HTTP_ARG_VAL(query_data, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addQueryData, 1)
	PHP_HTTP_ARG_VAL(query_data, 0)
PHP_HTTP_END_ARGS;


PHP_HTTP_EMPTY_ARGS(getBody);
PHP_HTTP_BEGIN_ARGS(setBody, 0)
	PHP_HTTP_ARG_OBJ(http\\Message\\Body, body, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(addBody, 1)
	PHP_HTTP_ARG_OBJ(http\\Message\\Body, body, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_BEGIN_ARGS(getResponseCookies, 0)
	PHP_HTTP_ARG_VAL(flags, 0)
	PHP_HTTP_ARG_VAL(allowed_extras, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseBody);
PHP_HTTP_EMPTY_ARGS(getResponseCode);
PHP_HTTP_EMPTY_ARGS(getResponseStatus);
PHP_HTTP_BEGIN_ARGS(getResponseHeader, 0)
	PHP_HTTP_ARG_VAL(header_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getMessageClass);
PHP_HTTP_BEGIN_ARGS(setMessageClass, 1)
	PHP_HTTP_ARG_VAL(message_class_name, 0)
PHP_HTTP_END_ARGS;

PHP_HTTP_EMPTY_ARGS(getResponseMessage);
PHP_HTTP_EMPTY_ARGS(getRequestMessage);
PHP_HTTP_EMPTY_ARGS(getHistory);
PHP_HTTP_EMPTY_ARGS(clearHistory);
PHP_HTTP_EMPTY_ARGS(send);

PHP_HTTP_EMPTY_ARGS(getObservers);
PHP_HTTP_BEGIN_ARGS(attach, 1)
	PHP_HTTP_ARG_OBJ(SplObserver, observer, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(detach, 1)
	PHP_HTTP_ARG_OBJ(SplObserver, observer, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_EMPTY_ARGS(notify);
PHP_HTTP_EMPTY_ARGS(getProgress);
PHP_HTTP_BEGIN_ARGS(getTransferInfo, 0)
	PHP_HTTP_ARG_VAL(name, 0)
PHP_HTTP_END_ARGS;


zend_class_entry *php_http_request_class_entry;
zend_function_entry php_http_request_method_entry[] = {
	PHP_HTTP_REQUEST_ME(__construct, ZEND_ACC_PRIVATE|ZEND_ACC_CTOR)

	PHP_HTTP_REQUEST_ME(getObservers, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(notify, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(attach, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(detach, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getProgress, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getTransferInfo, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(setSslOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getSslOptions, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(addSslOptions, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(addHeaders, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getHeaders, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(setHeaders, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(addCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(setCookies, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(enableCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(resetCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(flushCookies, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setMethod, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getMethod, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setUrl, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getUrl, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setContentType, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getContentType, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setQueryData, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getQueryData, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(addQueryData, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(setBody, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getBody, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(addBody, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(send, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(getResponseHeader, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseCookies, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseCode, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseStatus, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseBody, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getResponseMessage, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getRequestMessage, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(getHistory, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(clearHistory, ZEND_ACC_PUBLIC)

	PHP_HTTP_REQUEST_ME(getMessageClass, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_ME(setMessageClass, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers php_http_request_object_handlers;

zend_object_value php_http_request_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return php_http_request_object_new_ex(ce, NULL, NULL TSRMLS_CC);
}

zend_object_value php_http_request_object_new_ex(zend_class_entry *ce, php_http_request_t *r, php_http_request_object_t **ptr TSRMLS_DC)
{
	zend_object_value ov;
	php_http_request_object_t *o;

	o = ecalloc(1, sizeof(php_http_request_object_t));
	zend_object_std_init((zend_object *) o, ce TSRMLS_CC);
	object_properties_init((zend_object *) o, ce);

	if (!(o->request = r)) {
		o->request = php_http_request_init(NULL, NULL, NULL, NULL TSRMLS_CC);
	}

	if (ptr) {
		*ptr = o;
	}

	ov.handle = zend_objects_store_put(o, NULL, php_http_request_object_free, NULL TSRMLS_CC);
	ov.handlers = &php_http_request_object_handlers;

	return ov;
}

zend_object_value php_http_request_object_clone(zval *this_ptr TSRMLS_DC)
{
	zend_object_value new_ov;
	php_http_request_object_t *new_obj, *old_obj = zend_object_store_get_object(this_ptr TSRMLS_CC);

	new_ov = php_http_request_object_new_ex(old_obj->zo.ce, php_http_request_copy(old_obj->request, NULL), &new_obj TSRMLS_CC);
	zend_objects_clone_members(&new_obj->zo, new_ov, &old_obj->zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);

	return new_ov;
}

void php_http_request_object_free(void *object TSRMLS_DC)
{
	php_http_request_object_t *o = (php_http_request_object_t *) object;

	php_http_request_free(&o->request);
	zend_object_std_dtor((zend_object *) o TSRMLS_CC);
	efree(o);
}

static inline void php_http_request_object_check_request_content_type(zval *this_ptr TSRMLS_DC)
{
	zval *ctype = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("contentType"), 0 TSRMLS_CC);

	if (Z_STRLEN_P(ctype)) {
		zval **headers, *opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);

		if (	(Z_TYPE_P(opts) == IS_ARRAY) &&
				(SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), ZEND_STRS("headers"), (void *) &headers)) &&
				(Z_TYPE_PP(headers) == IS_ARRAY)) {
			zval **ct_header;

			/* only override if not already set */
			if ((SUCCESS != zend_hash_find(Z_ARRVAL_PP(headers), ZEND_STRS("Content-Type"), (void *) &ct_header))) {
				add_assoc_stringl(*headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
			} else
			/* or not a string, zero length string or a string of spaces */
			if ((Z_TYPE_PP(ct_header) != IS_STRING) || !Z_STRLEN_PP(ct_header)) {
				add_assoc_stringl(*headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
			} else {
				int i, only_space = 1;

				/* check for spaces only */
				for (i = 0; i < Z_STRLEN_PP(ct_header); ++i) {
					if (!PHP_HTTP_IS_CTYPE(space, Z_STRVAL_PP(ct_header)[i])) {
						only_space = 0;
						break;
					}
				}
				if (only_space) {
					add_assoc_stringl(*headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
				}
			}
		} else {
			zval *headers;

			MAKE_STD_ZVAL(headers);
			array_init(headers);
			add_assoc_stringl(headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
			zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addheaders", NULL, headers);
			zval_ptr_dtor(&headers);
		}
	}
}

static inline zend_object_value php_http_request_object_message(zval *this_ptr, php_http_message_t *msg TSRMLS_DC)
{
	zend_object_value ov;
	zval *zcn = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("messageClass"), 0 TSRMLS_CC);
	zend_class_entry *class_entry;

	if (Z_STRLEN_P(zcn)
	&&	(class_entry = zend_fetch_class(Z_STRVAL_P(zcn), Z_STRLEN_P(zcn), 0 TSRMLS_CC))
	&&	(SUCCESS == php_http_new(&ov, class_entry, (php_http_new_t) php_http_message_object_new_ex, php_http_message_class_entry, msg, NULL TSRMLS_CC))) {
		return ov;
	} else {
		return php_http_message_object_new_ex(php_http_message_class_entry, msg, NULL TSRMLS_CC);
	}
}

STATUS php_http_request_object_requesthandler(php_http_request_object_t *obj, zval *this_ptr, char **meth, char **url, php_http_message_body_t **body TSRMLS_DC)
{
	zval *zoptions;
	php_http_request_progress_t *progress;

	/* reset request handle */
	php_http_request_reset(obj->request);
	/* reset transfer info */
	zend_update_property_null(php_http_request_class_entry, getThis(), ZEND_STRL("info") TSRMLS_CC);

	if (meth) {
		*meth = Z_STRVAL_P(zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("method"), 0 TSRMLS_CC));
	}

	if (url) {
		php_url *tmp, qdu = {NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL};
		zval *zurl = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("url"), 0 TSRMLS_CC);
		zval *zqdata = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), 0 TSRMLS_CC);

		if (Z_STRLEN_P(zqdata)) {
			qdu.query = Z_STRVAL_P(zqdata);
		}
		php_http_url(0, tmp = php_url_parse(Z_STRVAL_P(zurl)), &qdu, NULL, url, NULL TSRMLS_CC);
		php_url_free(tmp);
	}

	if (body) {
		zval *zbody = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody"), 0 TSRMLS_CC);

		if (Z_TYPE_P(zbody) == IS_OBJECT) {
			*body = ((php_http_message_body_object_t *)zend_object_store_get_object(zbody TSRMLS_CC))->body;
			if (*body) {
				php_stream_rewind(php_http_message_body_stream(*body));
			}
		}
	}

	php_http_request_object_check_request_content_type(getThis() TSRMLS_CC);
	zoptions = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
	php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_SETTINGS, Z_ARRVAL_P(zoptions));

	if (SUCCESS == php_http_request_getopt(obj->request, PHP_HTTP_REQUEST_OPT_PROGRESS_INFO, &progress)) {
		if (!progress->callback) {
			php_http_request_progress_callback_t *callback = emalloc(sizeof(*callback));

			callback->type = PHP_HTTP_REQUEST_PROGRESS_CALLBACK_USER;
			callback->pass_state = 0;
			MAKE_STD_ZVAL(callback->func.user);
			array_init(callback->func.user);
			Z_ADDREF_P(getThis());
			add_next_index_zval(callback->func.user, getThis());
			add_next_index_stringl(callback->func.user, ZEND_STRL("notify"), 1);

			php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_PROGRESS_CALLBACK, callback);
		}
		progress->state.info = "start";
		php_http_request_progress_notify(progress TSRMLS_CC);
		progress->state.started = 1;
	}
	return SUCCESS;
}

static inline void empty_response(zval *this_ptr TSRMLS_DC)
{
	zend_update_property_null(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage") TSRMLS_CC);
	zend_update_property_long(php_http_request_class_entry, getThis(), ZEND_STRL("responseCode"), 0 TSRMLS_CC);
	zend_update_property_string(php_http_request_class_entry, getThis(), ZEND_STRL("responseStatus"), "" TSRMLS_CC);
}

STATUS php_http_request_object_responsehandler(php_http_request_object_t *obj, zval *this_ptr TSRMLS_DC)
{
	STATUS ret = SUCCESS;
	zval *info;
	php_http_message_t *msg;
	php_http_request_progress_t *progress;

	/* always fetch info */
	MAKE_STD_ZVAL(info);
	array_init(info);
	php_http_request_getopt(obj->request, PHP_HTTP_REQUEST_OPT_TRANSFER_INFO, Z_ARRVAL_P(info));
	zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("transferInfo"), info TSRMLS_CC);
	zval_ptr_dtor(&info);

	if ((msg = obj->request->message)) {
		/* update history */
		if (i_zend_is_true(zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("recordHistory"), 0 TSRMLS_CC))) {
			zval *new_hist, *old_hist = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("history"), 0 TSRMLS_CC);
			zend_object_value ov = php_http_request_object_message(getThis(), php_http_message_copy(msg, NULL) TSRMLS_CC);

			MAKE_STD_ZVAL(new_hist);
			ZVAL_OBJVAL(new_hist, ov, 0);

			if (Z_TYPE_P(old_hist) == IS_OBJECT) {
				php_http_message_object_prepend(new_hist, old_hist, 1 TSRMLS_CC);
			}

			zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("history"), new_hist TSRMLS_CC);
			zval_ptr_dtor(&new_hist);
		}

		/* update response info */
		if (PHP_HTTP_MESSAGE_TYPE(RESPONSE, msg)) {
			zval *message;

			zend_update_property_long(php_http_request_class_entry, getThis(), ZEND_STRL("responseCode"), msg->http.info.response.code TSRMLS_CC);
			zend_update_property_string(php_http_request_class_entry, getThis(), ZEND_STRL("responseStatus"), STR_PTR(msg->http.info.response.status) TSRMLS_CC);

			MAKE_STD_ZVAL(message);
			ZVAL_OBJVAL(message, php_http_request_object_message(getThis(), msg TSRMLS_CC), 0);
			zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), message TSRMLS_CC);
			zval_ptr_dtor(&message);

			obj->request->message = php_http_message_init(NULL, 0 TSRMLS_CC);
			msg = msg->parent;
		} else {
			empty_response(getThis() TSRMLS_CC);
		}
	} else {
		/* update properties with empty values */
		empty_response(getThis() TSRMLS_CC);
	}

	while (msg && !PHP_HTTP_MESSAGE_TYPE(REQUEST, msg)) {
		msg = msg->parent;
	}
	if (PHP_HTTP_MESSAGE_TYPE(REQUEST, msg)) {
		zval *message;

		MAKE_STD_ZVAL(message);
		ZVAL_OBJVAL(message, php_http_request_object_message(getThis(), php_http_message_copy_ex(msg, NULL, 0) TSRMLS_CC), 0);
		zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestMessage"), message TSRMLS_CC);
		zval_ptr_dtor(&message);
	}

	if (SUCCESS == php_http_request_getopt(obj->request, PHP_HTTP_REQUEST_OPT_PROGRESS_INFO, &progress)) {
		progress->state.info = "finished";
		progress->state.finished = 1;
		php_http_request_progress_notify(progress TSRMLS_CC);
	}
	php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_PROGRESS_CALLBACK, NULL);

	return ret;
}

static int apply_pretty_key(void *pDest TSRMLS_DC, int num_args, va_list args, zend_hash_key *hash_key)
{
	zval **zpp = pDest, *arr = va_arg(args, zval *);

	if (hash_key->arKey && hash_key->nKeyLength > 1) {
		char *tmp = php_http_pretty_key(estrndup(hash_key->arKey, hash_key->nKeyLength - 1), hash_key->nKeyLength - 1, 1, 0);

		Z_ADDREF_PP(zpp);
		add_assoc_zval_ex(arr, tmp, hash_key->nKeyLength, *zpp);
		efree(tmp);
	}
	return ZEND_HASH_APPLY_KEEP;
}

static inline void php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAMETERS, char *key, size_t len, int overwrite, int prettify_keys)
{
	zval *old_opts, *new_opts, *opts = NULL, **entry = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a/!", &opts)) {
		MAKE_STD_ZVAL(new_opts);
		array_init(new_opts);
		old_opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
		if (Z_TYPE_P(old_opts) == IS_ARRAY) {
			array_copy(Z_ARRVAL_P(old_opts), Z_ARRVAL_P(new_opts));
		}

		if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(new_opts), key, len, (void *) &entry)) {
			if (overwrite) {
				zend_hash_clean(Z_ARRVAL_PP(entry));
			}
			if (opts && zend_hash_num_elements(Z_ARRVAL_P(opts))) {
				if (overwrite) {
					array_copy(Z_ARRVAL_P(opts), Z_ARRVAL_PP(entry));
				} else {
					array_join(Z_ARRVAL_P(opts), Z_ARRVAL_PP(entry), 0, prettify_keys ? ARRAY_JOIN_PRETTIFY : 0);
				}
			}
		} else if (opts) {
			if (prettify_keys) {
				zval *tmp;

				MAKE_STD_ZVAL(tmp);
				array_init_size(tmp, zend_hash_num_elements(Z_ARRVAL_P(opts)));
				zend_hash_apply_with_arguments(Z_ARRVAL_P(opts) TSRMLS_CC, apply_pretty_key, 1, tmp);
				opts = tmp;
			} else {
				Z_ADDREF_P(opts);
			}
			add_assoc_zval_ex(new_opts, key, len, opts);
		}
		zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), new_opts TSRMLS_CC);
		zval_ptr_dtor(&new_opts);
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

static inline void php_http_request_object_get_options_subr(INTERNAL_FUNCTION_PARAMETERS, char *key, size_t len)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *opts, **options;

		opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
		array_init(return_value);

		if (	(Z_TYPE_P(opts) == IS_ARRAY) &&
				(SUCCESS == zend_symtable_find(Z_ARRVAL_P(opts), key, len, (void *) &options))) {
			convert_to_array(*options);
			array_copy(Z_ARRVAL_PP(options), Z_ARRVAL_P(return_value));
		}
	}
}


PHP_METHOD(HttpRequest, __construct)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		zend_parse_parameters_none();
	} end_error_handling();
}

PHP_METHOD(HttpRequest, getObservers)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			RETVAL_PROP(php_http_request_class_entry, "observers");
		}
	} end_error_handling();
}

static int notify(zend_object_iterator *iter, void *puser TSRMLS_DC)
{
	zval **observer = NULL;

	iter->funcs->get_current_data(iter, &observer TSRMLS_CC);
	if (observer) {
		zval *retval;

		zend_call_method_with_1_params(observer, NULL, NULL, "update", &retval, puser);
		zval_ptr_dtor(&retval);
		return SUCCESS;
	}
	return FAILURE;
}

PHP_METHOD(HttpRequest, notify)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zval *observers = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);

		if (Z_TYPE_P(observers) == IS_OBJECT) {
			Z_ADDREF_P(getThis());
			spl_iterator_apply(observers, notify, getThis() TSRMLS_CC);
			zval_ptr_dtor(&getThis());
		}
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, attach)
{
	zval *observer;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &observer, spl_ce_SplObserver)) {
		zval *retval, *observers = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);
		zend_call_method_with_1_params(&observers, NULL, NULL, "attach", &retval, observer);
		zval_ptr_dtor(&retval);
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, detach)
{
	zval *observer;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &observer, spl_ce_SplObserver)) {
		zval *retval, *observers = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("observers"), 0 TSRMLS_CC);
		zend_call_method_with_1_params(&observers, NULL, NULL, "detach", &retval, observer);
		zval_ptr_dtor(&retval);
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, getProgress)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		php_http_request_progress_t *progress;

		php_http_request_getopt(obj->request, PHP_HTTP_REQUEST_OPT_PROGRESS_INFO, &progress);
		object_init(return_value);
		add_property_bool(return_value, "started", progress->state.started);
		add_property_bool(return_value, "finished", progress->state.finished);
		add_property_string(return_value, "info", STR_PTR(progress->state.info), 1);
		add_property_double(return_value, "dltotal", progress->state.dl.total);
		add_property_double(return_value, "dlnow", progress->state.dl.now);
		add_property_double(return_value, "ultotal", progress->state.ul.total);
		add_property_double(return_value, "ulnow", progress->state.ul.now);
	}
}

PHP_METHOD(HttpRequest, getTransferInfo)
{
	char *info_name = NULL;
	int info_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &info_name, &info_len)) {
		zval **infop, *temp = NULL, *info = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("transferInfo"), 0 TSRMLS_CC);

		/* request completed? */
		if (Z_TYPE_P(info) != IS_ARRAY) {
			php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

			MAKE_STD_ZVAL(temp);
			array_init(temp);
			php_http_request_getopt(obj->request, PHP_HTTP_REQUEST_OPT_TRANSFER_INFO, Z_ARRVAL_P(temp));
			info = temp;
		}

		if (info_len && info_name) {
			if (SUCCESS == zend_symtable_find(Z_ARRVAL_P(info), php_http_pretty_key(info_name, info_len, 0, 0), info_len + 1, (void *) &infop)) {
				RETVAL_ZVAL(*infop, 1, 0);
			} else {
				php_http_error(HE_NOTICE, PHP_HTTP_E_INVALID_PARAM, "Could not find transfer info named %s", info_name);
				RETVAL_FALSE;
			}
		} else {
			RETVAL_ZVAL(info, 1, 0);
		}

		if (temp) {
			zval_ptr_dtor(&temp);
		}
		return;
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setOptions)
{
	php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
	HashPosition pos;
	zval *opts = NULL, *old_opts, *new_opts, *add_opts, **opt;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		RETURN_FALSE;
	}

	MAKE_STD_ZVAL(new_opts);
	array_init(new_opts);

	if (!opts || !zend_hash_num_elements(Z_ARRVAL_P(opts))) {
		zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), new_opts TSRMLS_CC);
		zval_ptr_dtor(&new_opts);
	} else {
		MAKE_STD_ZVAL(add_opts);
		array_init(add_opts);
		/* some options need extra attention -- thus cannot use array_merge() directly */
		FOREACH_KEYVAL(pos, opts, key, opt) {
			if (key.type == HASH_KEY_IS_STRING) {
#define KEYMATCH(k, s) ((sizeof(s)==k.len) && !strcasecmp(k.str, s))
				if (KEYMATCH(key, "headers")) {
					zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addheaders", NULL, *opt);
				} else if (KEYMATCH(key, "cookies")) {
					zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addcookies", NULL, *opt);
				} else if (KEYMATCH(key, "ssl")) {
					zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addssloptions", NULL, *opt);
				} else if (KEYMATCH(key, "url") || KEYMATCH(key, "uri")) {
					zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "seturl", NULL, *opt);
				} else if (KEYMATCH(key, "method")) {
					zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "setmethod", NULL, *opt);
				} else if (KEYMATCH(key, "flushcookies")) {
					if (i_zend_is_true(*opt)) {
						php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

						php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_COOKIES_FLUSH, NULL);
					}
				} else if (KEYMATCH(key, "resetcookies")) {
					php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

					php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_COOKIES_RESET, NULL);
				} else if (KEYMATCH(key, "enablecookies")) {
					php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

					php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_COOKIES_ENABLE, NULL);
				} else if (KEYMATCH(key, "recordHistory")) {
					zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("recordHistory"), *opt TSRMLS_CC);
				} else if (KEYMATCH(key, "messageClass")) {
					zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "setmessageclass", NULL, *opt);
				} else if (Z_TYPE_PP(opt) == IS_NULL) {
					old_opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
					if (Z_TYPE_P(old_opts) == IS_ARRAY) {
						zend_symtable_del(Z_ARRVAL_P(old_opts), key.str, key.len);
					}
				} else {
					Z_ADDREF_P(*opt);
					add_assoc_zval_ex(add_opts, key.str, key.len, *opt);
				}
			}
		}

		old_opts = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), 0 TSRMLS_CC);
		if (Z_TYPE_P(old_opts) == IS_ARRAY) {
			array_copy(Z_ARRVAL_P(old_opts), Z_ARRVAL_P(new_opts));
		}
		array_join(Z_ARRVAL_P(add_opts), Z_ARRVAL_P(new_opts), 0, 0);
		zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("options"), new_opts TSRMLS_CC);
		zval_ptr_dtor(&new_opts);
		zval_ptr_dtor(&add_opts);
	}

	RETVAL_ZVAL(getThis(), 1, 0);
}



PHP_METHOD(HttpRequest, getOptions)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "options");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setSslOptions)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("ssl"), 1, 0);
}

PHP_METHOD(HttpRequest, addSslOptions)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("ssl"), 0, 0);
}

PHP_METHOD(HttpRequest, getSslOptions)
{
	php_http_request_object_get_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("ssl"));
}

PHP_METHOD(HttpRequest, addHeaders)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("headers"), 0, 1);
}

PHP_METHOD(HttpRequest, setHeaders)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("headers"), 1, 1);
}

PHP_METHOD(HttpRequest, getHeaders)
{
	php_http_request_object_get_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("headers"));
}

PHP_METHOD(HttpRequest, setCookies)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("cookies"), 1, 0);
}

PHP_METHOD(HttpRequest, addCookies)
{
	php_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("cookies"), 0, 0);
}

PHP_METHOD(HttpRequest, getCookies)
{
	php_http_request_object_get_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, ZEND_STRS("cookies"));
}

PHP_METHOD(HttpRequest, enableCookies)
{
	if (SUCCESS == zend_parse_parameters_none()){
		php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_COOKIES_ENABLE, NULL);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, resetCookies)
{
	zend_bool session_only = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &session_only)) {
		php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
		if (session_only) {
			php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_COOKIES_RESET_SESSION, NULL);
		} else {
			php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_COOKIES_RESET, NULL);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, flushCookies)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);

		php_http_request_setopt(obj->request, PHP_HTTP_REQUEST_OPT_COOKIES_FLUSH, NULL);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, setUrl)
{
	char *url_str = NULL;
	int url_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &url_str, &url_len)) {
		zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("url"), url_str, url_len TSRMLS_CC);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, getUrl)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "url");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setMethod)
{
	char *meth_str;
	int meth_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &meth_str, &meth_len)) {
		zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("method"), meth_str, meth_len TSRMLS_CC);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, getMethod)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "method");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setContentType)
{
	char *ctype;
	int ct_len;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ct_len)) {
		int invalid = 0;

		if (ct_len) {
			PHP_HTTP_CHECK_CONTENT_TYPE(ctype, invalid = 1);
		}

		if (!invalid) {
			zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("contentType"), ctype, ct_len TSRMLS_CC);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, getContentType)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "contentType");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setQueryData)
{
	zval *qdata = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z!", &qdata)) {
		if ((!qdata) || Z_TYPE_P(qdata) == IS_NULL) {
			zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), "", 0 TSRMLS_CC);
		} else if ((Z_TYPE_P(qdata) == IS_ARRAY) || (Z_TYPE_P(qdata) == IS_OBJECT)) {
			char *query_data_str = NULL;
			size_t query_data_len;

			if (SUCCESS == php_http_url_encode_hash(HASH_OF(qdata), NULL, 0, &query_data_str, &query_data_len TSRMLS_CC)) {
				zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), query_data_str, query_data_len TSRMLS_CC);
				efree(query_data_str);
			}
		} else {
			zval *data = php_http_ztyp(IS_STRING, qdata);

			zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), Z_STRVAL_P(data), Z_STRLEN_P(data) TSRMLS_CC);
			zval_ptr_dtor(&data);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, getQueryData)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "queryData");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, addQueryData)
{
	zval *qdata;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &qdata)) {
		char *query_data_str = NULL;
		size_t query_data_len = 0;
		zval *old_qdata = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), 0 TSRMLS_CC);

		if (SUCCESS == php_http_url_encode_hash(HASH_OF(qdata), Z_STRVAL_P(old_qdata), Z_STRLEN_P(old_qdata), &query_data_str, &query_data_len TSRMLS_CC)) {
			zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("queryData"), query_data_str, query_data_len TSRMLS_CC);
			efree(query_data_str);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);

}

PHP_METHOD(HttpRequest, setBody)
{
	zval *body = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|O!", &body, php_http_message_body_class_entry)) {
		if (body && Z_TYPE_P(body) != IS_NULL) {
			zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody"), body TSRMLS_CC);
		} else {
			zend_update_property_null(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody") TSRMLS_CC);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, addBody)
{
	zval *new_body;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O", &new_body, php_http_message_body_class_entry)) {
		zval *old_body = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody"), 0 TSRMLS_CC);

		if (Z_TYPE_P(old_body) == IS_OBJECT) {
			php_http_message_body_object_t *old_obj = zend_object_store_get_object(old_body TSRMLS_CC);
			php_http_message_body_object_t *new_obj = zend_object_store_get_object(new_body TSRMLS_CC);

			php_http_message_body_to_callback(old_obj->body, (php_http_pass_callback_t) php_http_message_body_append, new_obj->body, 0, 0);
		} else {
			zend_update_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestBody"), new_body TSRMLS_CC);
		}
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, getBody)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "requestBody");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseHeader)
{
	zval *header;
	char *header_name = NULL;
	int header_len = 0;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &header_name, &header_len)) {
		zval *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), 0 TSRMLS_CC);

		if (Z_TYPE_P(message) == IS_OBJECT) {
			php_http_message_object_t *msg = zend_object_store_get_object(message TSRMLS_CC);

			if (header_len) {
				if ((header = php_http_message_header(msg->message, header_name, header_len + 1, 0))) {
					RETURN_ZVAL(header, 1, 1);
				}
			} else {
				array_init(return_value);
				zend_hash_copy(Z_ARRVAL_P(return_value), &msg->message->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
				return;
			}
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseCookies)
{
	long flags = 0;
	zval *allowed_extras_array = NULL;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|la!", &flags, &allowed_extras_array)) {
		int i = 0;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);
		char **allowed_extras = NULL;
		zval **header = NULL, **entry = NULL, *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), 0 TSRMLS_CC);
		HashPosition pos, pos1, pos2;

		if (Z_TYPE_P(message) == IS_OBJECT) {
			php_http_message_object_t *msg = zend_object_store_get_object(message TSRMLS_CC);

			array_init(return_value);

			if (allowed_extras_array) {
				allowed_extras = ecalloc(zend_hash_num_elements(Z_ARRVAL_P(allowed_extras_array)) + 1, sizeof(char *));
				FOREACH_VAL(pos, allowed_extras_array, entry) {
					zval *data = php_http_ztyp(IS_STRING, *entry);
					allowed_extras[i++] = estrndup(Z_STRVAL_P(data), Z_STRLEN_P(data));
					zval_ptr_dtor(&data);
				}
			}

			FOREACH_HASH_KEYVAL(pos1, &msg->message->hdrs, key, header) {
				if (key.type == HASH_KEY_IS_STRING && !strcasecmp(key.str, "Set-Cookie")) {
					php_http_cookie_list_t *list;

					if (Z_TYPE_PP(header) == IS_ARRAY) {
						zval **single_header;

						FOREACH_VAL(pos2, *header, single_header) {
							zval *data = php_http_ztyp(IS_STRING, *single_header);

							if ((list = php_http_cookie_list_parse(NULL, Z_STRVAL_P(data), Z_STRLEN_P(data), flags, allowed_extras TSRMLS_CC))) {
								zval *cookie;

								MAKE_STD_ZVAL(cookie);
								ZVAL_OBJVAL(cookie, php_http_cookie_object_new_ex(php_http_cookie_class_entry, list, NULL TSRMLS_CC), 0);
								add_next_index_zval(return_value, cookie);
							}
							zval_ptr_dtor(&data);
						}
					} else {
						zval *data = php_http_ztyp(IS_STRING, *header);
						if ((list = php_http_cookie_list_parse(NULL, Z_STRVAL_P(data), Z_STRLEN_P(data), flags, allowed_extras TSRMLS_CC))) {
							zval *cookie;

							MAKE_STD_ZVAL(cookie);
							ZVAL_OBJVAL(cookie, php_http_cookie_object_new_ex(php_http_cookie_class_entry, list, NULL TSRMLS_CC), 0);
							add_next_index_zval(return_value, cookie);
						}
						zval_ptr_dtor(&data);
					}
				}
			}

			if (allowed_extras) {
				for (i = 0; allowed_extras[i]; ++i) {
					efree(allowed_extras[i]);
				}
				efree(allowed_extras);
			}

			return;
		}
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseBody)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), 0 TSRMLS_CC);

			if (Z_TYPE_P(message) == IS_OBJECT) {
				RETURN_OBJVAL(((php_http_message_object_t *)zend_object_store_get_object(message TSRMLS_CC))->body, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "HttpRequest does not contain a response message");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequest, getResponseCode)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "responseCode");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseStatus)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "responseStatus");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, getResponseMessage)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("responseMessage"), 0 TSRMLS_CC);

			if (Z_TYPE_P(message) == IS_OBJECT) {
				RETVAL_OBJECT(message, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "HttpRequest does not contain a response message");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequest, getRequestMessage)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *message = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("requestMessage"), 0 TSRMLS_CC);

			if (Z_TYPE_P(message) == IS_OBJECT) {
				RETVAL_OBJECT(message, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "HttpRequest does not contain a request message");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequest, getHistory)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			zval *hist = zend_read_property(php_http_request_class_entry, getThis(), ZEND_STRL("history"), 0 TSRMLS_CC);

			if (Z_TYPE_P(hist) == IS_OBJECT) {
				RETVAL_OBJECT(hist, 1);
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_RUNTIME, "The history is empty");
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequest, clearHistory)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		zend_update_property_null(php_http_request_class_entry, getThis(), ZEND_STRL("history") TSRMLS_CC);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, getMessageClass)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_class_entry, "messageClass");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequest, setMessageClass)
{
	char *cn;
	int cl;

	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &cn, &cl)) {
		zend_update_property_stringl(php_http_request_class_entry, getThis(), ZEND_STRL("messageClass"), cn, cl TSRMLS_CC);
	}
	RETVAL_ZVAL(getThis(), 1, 0);
}

PHP_METHOD(HttpRequest, send)
{
	RETVAL_FALSE;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters_none()) {
			php_http_request_object_t *obj = zend_object_store_get_object(getThis() TSRMLS_CC);
			php_http_message_body_t *body = NULL;
			char *meth = NULL;
			char *url = NULL;

			if (SUCCESS == php_http_request_object_requesthandler(obj, getThis(), &meth, &url, &body TSRMLS_CC)) {
				php_http_request_exec(obj->request, meth, url, body);
				if (SUCCESS == php_http_request_object_responsehandler(obj, getThis() TSRMLS_CC)) {
					RETVAL_PROP(php_http_request_class_entry, "responseMessage");
				} else {
					php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Failed to handle response");
				}
			} else {
				php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST, "Failed to handle request");
			}

			STR_FREE(url);
		}
	} end_error_handling();
}

PHP_MINIT_FUNCTION(http_request)
{
	PHP_HTTP_REGISTER_CLASS(http, Request, http_request, php_http_object_class_entry, 0);
	php_http_request_class_entry->create_object = php_http_request_object_new;
	memcpy(&php_http_request_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_http_request_object_handlers.clone_obj = php_http_request_object_clone;

	zend_class_implements(php_http_request_class_entry TSRMLS_CC, 1, spl_ce_SplSubject);

	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("observers"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("options"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("transferInfo"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("responseMessage"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_long(php_http_request_class_entry, ZEND_STRL("responseCode"), 0, ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("responseStatus"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("requestMessage"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("method"), "GET", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("url"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("contentType"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("requestBody"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("queryData"), "", ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_null(php_http_request_class_entry, ZEND_STRL("history"), ZEND_ACC_PRIVATE TSRMLS_CC);
	zend_declare_property_bool(php_http_request_class_entry, ZEND_STRL("recordHistory"), 0, ZEND_ACC_PUBLIC TSRMLS_CC);
	zend_declare_property_string(php_http_request_class_entry, ZEND_STRL("messageClass"), "", ZEND_ACC_PRIVATE TSRMLS_CC);

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

