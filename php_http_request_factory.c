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

/*
 * array of name => php_http_request_factory_driver_t*
 */
static HashTable php_http_request_factory_drivers;

PHP_HTTP_API STATUS php_http_request_factory_add_driver(const char *name_str, size_t name_len, php_http_request_factory_driver_t *driver)
{
	return zend_hash_add(&php_http_request_factory_drivers, name_str, name_len + 1, (void *) driver, sizeof(php_http_request_factory_driver_t), NULL);
}

PHP_HTTP_API STATUS php_http_request_factory_get_driver(const char *name_str, size_t name_len, php_http_request_factory_driver_t *driver)
{
	php_http_request_factory_driver_t *tmp;

	if (SUCCESS == zend_hash_find(&php_http_request_factory_drivers, name_str, name_len + 1, (void *) &tmp)) {
		*driver = *tmp;
		return SUCCESS;
	}
	return FAILURE;
}

static zend_class_entry *php_http_request_factory_get_class_entry(zval *this_ptr, const char *for_str, size_t for_len TSRMLS_DC)
{
	/* stupid non-const api */
	char *sc = estrndup(for_str, for_len);
	zval *cn = zend_read_property(Z_OBJCE_P(getThis()), getThis(), sc, for_len, 0 TSRMLS_CC);

	efree(sc);
	if (Z_TYPE_P(cn) == IS_STRING && Z_STRLEN_P(cn)) {
		return zend_fetch_class(Z_STRVAL_P(cn), Z_STRLEN_P(cn), 0 TSRMLS_CC);
	}

	return NULL;
}

#define PHP_HTTP_BEGIN_ARGS(method, req_args) 	PHP_HTTP_BEGIN_ARGS_EX(HttpRequestFactory, method, 0, req_args)
#define PHP_HTTP_EMPTY_ARGS(method)				PHP_HTTP_EMPTY_ARGS_EX(HttpRequestFactory, method, 0)
#define PHP_HTTP_REQUEST_FACTORY_ME(method, visibility)	PHP_ME(HttpRequestFactory, method, PHP_HTTP_ARGS(HttpRequestFactory, method), visibility)
#define PHP_HTTP_REQUEST_FACTORY_ALIAS(method, func)	PHP_HTTP_STATIC_ME_ALIAS(method, func, PHP_HTTP_ARGS(HttpRequestFactory, method))
#define PHP_HTTP_REQUEST_FACTORY_MALIAS(me, al, vis)	ZEND_FENTRY(me, ZEND_MN(HttpRequestFactory_##al), PHP_HTTP_ARGS(HttpRequestFactory, al), vis)

PHP_HTTP_BEGIN_ARGS(__construct, 1)
	PHP_HTTP_ARG_VAL(options, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(createRequest, 0)
	PHP_HTTP_ARG_VAL(url, 0)
	PHP_HTTP_ARG_VAL(method, 0)
	PHP_HTTP_ARG_VAL(options, 0)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(createPool, 0)
	PHP_HTTP_ARG_OBJ(http\\Request, request1, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, request2, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, requestN, 1)
PHP_HTTP_END_ARGS;
PHP_HTTP_BEGIN_ARGS(createDataShare, 0)
	PHP_HTTP_ARG_OBJ(http\\Request, request1, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, request2, 1)
	PHP_HTTP_ARG_OBJ(http\\Request, requestN, 1)
PHP_HTTP_END_ARGS;
PHP_HTTP_EMPTY_ARGS(getDriver);
PHP_HTTP_EMPTY_ARGS(getAvailableDrivers);

zend_class_entry *php_http_request_factory_class_entry;
zend_function_entry php_http_request_factory_method_entry[] = {
	PHP_HTTP_REQUEST_FACTORY_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_HTTP_REQUEST_FACTORY_ME(createRequest, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_FACTORY_ME(createPool, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_FACTORY_ME(createDataShare, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_FACTORY_ME(getDriver, ZEND_ACC_PUBLIC)
	PHP_HTTP_REQUEST_FACTORY_ME(getAvailableDrivers, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)

	EMPTY_FUNCTION_ENTRY
};

PHP_METHOD(HttpRequestFactory, __construct)
{
	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		HashTable *options = NULL;

		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|h", &options)) {
			if (options) {
				zval **val;
				HashPosition pos;
				php_http_array_hashkey_t key = php_http_array_hashkey_init(0);

				FOREACH_HASH_KEYVAL(pos, options, key, val) {
					if (key.type == HASH_KEY_IS_STRING) {
						zval *newval = php_http_zsep(1, Z_TYPE_PP(val), *val);
						zend_update_property(php_http_request_factory_class_entry, getThis(), key.str, key.len - 1, newval TSRMLS_CC);
						zval_ptr_dtor(&newval);
					}
				}
			}
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequestFactory, createRequest)
{
	char *meth_str = NULL, *url_str = NULL;
	int meth_len, url_len;
	zval *options = NULL;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s!s!a!", &url_str, &url_len, &meth_str, &meth_len, &options)) {
			with_error_handling(EH_THROW, php_http_exception_class_entry) {
				zval *zdriver, *os;
				zend_object_value ov;
				zend_class_entry *class_entry = NULL;
				php_http_client_t *req = NULL;
				php_http_request_factory_driver_t driver;

				class_entry = php_http_request_factory_get_class_entry(getThis(), ZEND_STRL("requestClass") TSRMLS_CC);

				if (!class_entry) {
					class_entry = php_http_client_class_entry;
				}

				zdriver = zend_read_property(php_http_request_factory_class_entry, getThis(), ZEND_STRL("driver"), 0 TSRMLS_CC);

				if ((IS_STRING == Z_TYPE_P(zdriver)) && (SUCCESS == php_http_request_factory_get_driver(Z_STRVAL_P(zdriver), Z_STRLEN_P(zdriver), &driver)) && driver.request_ops) {
					zval *phi = php_http_zsep(1, IS_STRING, zend_read_property(php_http_request_factory_class_entry, getThis(), ZEND_STRL("persistentHandleId"), 0 TSRMLS_CC));
					php_http_resource_factory_t *rf = NULL;

					if (Z_STRLEN_P(phi)) {
						char *name_str;
						size_t name_len;
						php_http_persistent_handle_factory_t *pf;

						name_len = spprintf(&name_str, 0, "http_request.%s", Z_STRVAL_P(zdriver));

						if ((pf = php_http_persistent_handle_concede(NULL , name_str, name_len, Z_STRVAL_P(phi), Z_STRLEN_P(phi) TSRMLS_CC))) {
							rf = php_http_resource_factory_init(NULL, php_http_persistent_handle_resource_factory_ops(), pf, (void (*)(void *)) php_http_persistent_handle_abandon);
						}

						efree(name_str);
					}

					req = php_http_client_init(NULL, driver.request_ops, rf, NULL TSRMLS_CC);
					if (req) {
						if (SUCCESS == php_http_new(&ov, class_entry, (php_http_new_t) php_http_client_object_new_ex, php_http_client_class_entry, req, NULL TSRMLS_CC)) {
							ZVAL_OBJVAL(return_value, ov, 0);

							MAKE_STD_ZVAL(os);
							object_init_ex(os, spl_ce_SplObjectStorage);
							zend_update_property(php_http_client_class_entry, return_value, ZEND_STRL("observers"), os TSRMLS_CC);
							zval_ptr_dtor(&os);

							if (url_str) {
								zend_update_property_stringl(php_http_client_class_entry, return_value, ZEND_STRL("url"), url_str, url_len TSRMLS_CC);
							}
							if (meth_str) {
								zend_update_property_stringl(php_http_client_class_entry, return_value, ZEND_STRL("method"), meth_str, meth_len TSRMLS_CC);
							}
							if (options) {
								zend_call_method_with_1_params(&return_value, Z_OBJCE_P(return_value), NULL, "setoptions", NULL, options);
							}
						} else {
							php_http_client_free(&req);
						}
					}

					zval_ptr_dtor(&phi);
				} else {
					php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_FACTORY, "requests are not supported by this driver");
				}
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequestFactory, createPool)
{
	int argc = 0;
	zval ***argv;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|*", &argv, &argc)) {
			with_error_handling(EH_THROW, php_http_exception_class_entry) {
				int i;
				zval *zdriver;
				zend_object_value ov;
				zend_class_entry *class_entry = NULL;
				php_http_request_pool_t *pool = NULL;
				php_http_request_factory_driver_t driver;

				if (!(class_entry = php_http_request_factory_get_class_entry(getThis(), ZEND_STRL("requestPoolClass") TSRMLS_CC))) {
					class_entry = php_http_request_pool_class_entry;
				}

				zdriver = zend_read_property(php_http_request_factory_class_entry, getThis(), ZEND_STRL("driver"), 0 TSRMLS_CC);
				if ((IS_STRING == Z_TYPE_P(zdriver)) && (SUCCESS == php_http_request_factory_get_driver(Z_STRVAL_P(zdriver), Z_STRLEN_P(zdriver), &driver)) && driver.request_pool_ops) {
					zval *phi = php_http_zsep(1, IS_STRING, zend_read_property(php_http_request_factory_class_entry, getThis(), ZEND_STRL("persistentHandleId"), 0 TSRMLS_CC));
					php_http_resource_factory_t *rf = NULL;

					if (Z_STRLEN_P(phi)) {
						char *name_str;
						size_t name_len;
						php_http_persistent_handle_factory_t *pf;

						name_len = spprintf(&name_str, 0, "http_request_pool.%s", Z_STRVAL_P(zdriver));

						if ((pf = php_http_persistent_handle_concede(NULL , name_str, name_len, Z_STRVAL_P(phi), Z_STRLEN_P(phi) TSRMLS_CC))) {
							rf = php_http_resource_factory_init(NULL, php_http_persistent_handle_resource_factory_ops(), pf, (void (*)(void *)) php_http_persistent_handle_abandon);
						}

						efree(name_str);
					}

					pool = php_http_request_pool_init(NULL, driver.request_pool_ops, rf, NULL TSRMLS_CC);
					if (pool) {
						if (SUCCESS == php_http_new(&ov, class_entry, (php_http_new_t) php_http_request_pool_object_new_ex, php_http_request_pool_class_entry, pool, NULL TSRMLS_CC)) {
							ZVAL_OBJVAL(return_value, ov, 0);
							for (i = 0; i < argc; ++i) {
								if (Z_TYPE_PP(argv[i]) == IS_OBJECT && instanceof_function(Z_OBJCE_PP(argv[i]), php_http_client_class_entry TSRMLS_CC)) {
									php_http_request_pool_attach(pool, *(argv[i]));
								}
							}
						} else {
							php_http_request_pool_free(&pool);
						}
					}

					zval_ptr_dtor(&phi);
				} else {
					php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_FACTORY, "pools are not supported by this driver");
				}
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequestFactory, createDataShare)
{
	int argc = 0;
	zval ***argv;

	with_error_handling(EH_THROW, php_http_exception_class_entry) {
		if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|*", &argv, &argc)) {
			with_error_handling(EH_THROW, php_http_exception_class_entry) {
				int i;
				zval *zdriver;
				zend_object_value ov;
				zend_class_entry *class_entry;
				php_http_client_datashare_t *share = NULL;
				php_http_request_factory_driver_t driver;

				if (!(class_entry = php_http_request_factory_get_class_entry(getThis(), ZEND_STRL("requestDataShareClass") TSRMLS_CC))) {
					class_entry = php_http_request_datashare_class_entry;
				}

				zdriver = zend_read_property(php_http_request_factory_class_entry, getThis(), ZEND_STRL("driver"), 0 TSRMLS_CC);
				if ((IS_STRING == Z_TYPE_P(zdriver)) && (SUCCESS == php_http_request_factory_get_driver(Z_STRVAL_P(zdriver), Z_STRLEN_P(zdriver), &driver)) && driver.request_datashare_ops) {
					zval *phi = php_http_zsep(1, IS_STRING, zend_read_property(php_http_request_factory_class_entry, getThis(), ZEND_STRL("persistentHandleId"), 0 TSRMLS_CC));
					php_http_resource_factory_t *rf = NULL;

					if (Z_STRLEN_P(phi)) {
						char *name_str;
						size_t name_len;
						php_http_persistent_handle_factory_t *pf;

						name_len = spprintf(&name_str, 0, "http_request_datashare.%s", Z_STRVAL_P(zdriver));

						if ((pf = php_http_persistent_handle_concede(NULL , name_str, name_len, Z_STRVAL_P(phi), Z_STRLEN_P(phi) TSRMLS_CC))) {
							rf = php_http_resource_factory_init(NULL, php_http_persistent_handle_resource_factory_ops(), pf, (void (*)(void *)) php_http_persistent_handle_abandon);
						}

						efree(name_str);
					}

					share = php_http_client_datashare_init(NULL, driver.request_datashare_ops, rf, NULL TSRMLS_CC);
					if (share) {
						if (SUCCESS == php_http_new(&ov, class_entry, (php_http_new_t) php_http_request_datashare_object_new_ex, php_http_request_datashare_class_entry, share, NULL TSRMLS_CC)) {
							ZVAL_OBJVAL(return_value, ov, 0);
							for (i = 0; i < argc; ++i) {
								if (Z_TYPE_PP(argv[i]) == IS_OBJECT && instanceof_function(Z_OBJCE_PP(argv[i]), php_http_client_class_entry TSRMLS_CC)) {
									php_http_request_datashare_attach(share, *(argv[i]));
								}
							}
						} else {
							php_http_request_datashare_free(&share);
						}
					}

					zval_ptr_dtor(&phi);
				} else {
					php_http_error(HE_WARNING, PHP_HTTP_E_REQUEST_FACTORY, "datashares are not supported by this driver");
				}
			} end_error_handling();
		}
	} end_error_handling();
}

PHP_METHOD(HttpRequestFactory, getDriver)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		RETURN_PROP(php_http_request_factory_class_entry, "driver");
	}
	RETURN_FALSE;
}

PHP_METHOD(HttpRequestFactory, getAvailableDrivers)
{
	if (SUCCESS == zend_parse_parameters_none()) {
		HashPosition pos;
		php_http_array_hashkey_t key = php_http_array_hashkey_init(0);

		array_init(return_value);
		FOREACH_HASH_KEY(pos, &php_http_request_factory_drivers, key) {
			add_next_index_stringl(return_value, key.str, key.len - 1, 1);
		}
		return;
	}
	RETURN_FALSE;
}

PHP_MINIT_FUNCTION(http_request_factory)
{
	zend_hash_init(&php_http_request_factory_drivers, 0, NULL, NULL, 1);

	PHP_HTTP_REGISTER_CLASS(http\\Request, Factory, http_request_factory, php_http_object_class_entry, 0);
	php_http_request_factory_class_entry->create_object = php_http_request_factory_new;

	zend_declare_property_stringl(php_http_request_factory_class_entry, ZEND_STRL("driver"), ZEND_STRL("curl"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_request_factory_class_entry, ZEND_STRL("persistentHandleId"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_request_factory_class_entry, ZEND_STRL("requestClass"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_request_factory_class_entry, ZEND_STRL("requestPoolClass"), ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(php_http_request_factory_class_entry, ZEND_STRL("requestDataShareClass"), ZEND_ACC_PROTECTED TSRMLS_CC);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(http_request_factory)
{
	zend_hash_destroy(&php_http_request_factory_drivers);

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

