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
#include "php_http_std_defs.h"

#ifdef ZEND_ENGINE_2

#include "php_http_exception_object.h"
#include "zend_exceptions.h"

zend_class_entry *http_exception_object_ce;
zend_function_entry http_exception_object_fe[] = {{NULL, NULL, NULL}};

void _http_exception_object_init(INIT_FUNC_ARGS)
{
	HTTP_REGISTER_CLASS(HttpException, http_exception_object, zend_exception_get_default(), 0);
	
	HTTP_LONG_CONSTANT("HTTP_E_UNKNOWN", HTTP_E_UNKOWN);
	HTTP_LONG_CONSTANT("HTTP_E_PARSE", HTTP_E_PARSE);
	HTTP_LONG_CONSTANT("HTTP_E_HEADER", HTTP_E_HEADER);
	HTTP_LONG_CONSTANT("HTTP_E_OBUFFER", HTTP_E_OBUFFER);
	HTTP_LONG_CONSTANT("HTTP_E_CURL", HTTP_E_CURL);
	HTTP_LONG_CONSTANT("HTTP_E_ENCODE", HTTP_E_ENCODE);
	HTTP_LONG_CONSTANT("HTTP_E_PARAM", HTTP_E_PARAM);
	HTTP_LONG_CONSTANT("HTTP_E_URL", HTTP_E_URL);
	HTTP_LONG_CONSTANT("HTTP_E_MSG", HTTP_E_MSG);
}

zend_class_entry *_http_exception_get_default()
{
	return http_exception_object_ce;
}

zend_class_entry *_http_exception_get_for_code(long code)
{
	return http_exception_object_ce;
}

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

