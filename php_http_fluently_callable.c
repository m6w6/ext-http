
#include "php_http.h"

zend_class_entry *php_http_fluently_callable_class_entry;
zend_function_entry php_http_fluently_callable_method_entry[] = {
	EMPTY_FUNCTION_ENTRY
};

PHP_MINIT_FUNCTION(http_fluently_callable)
{
	PHP_HTTP_REGISTER_INTERFACE(http, FluentlyCallable, http_fluently_callable, 0);
	return SUCCESS;
}
