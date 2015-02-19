/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2015, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

#ifndef PHP_HTTP_RESPONSE_CODE
#	define PHP_HTTP_RESPONSE_CODE(code, status)
#endif

PHP_HTTP_RESPONSE_CODE(100, "Continue")
PHP_HTTP_RESPONSE_CODE(101, "Switching Protocols")
PHP_HTTP_RESPONSE_CODE(102, "Processing")
PHP_HTTP_RESPONSE_CODE(200, "OK")
PHP_HTTP_RESPONSE_CODE(201, "Created")
PHP_HTTP_RESPONSE_CODE(202, "Accepted")
PHP_HTTP_RESPONSE_CODE(203, "Non-Authoritative Information")
PHP_HTTP_RESPONSE_CODE(204, "No Content")
PHP_HTTP_RESPONSE_CODE(205, "Reset Content")
PHP_HTTP_RESPONSE_CODE(206, "Partial Content")
PHP_HTTP_RESPONSE_CODE(207, "Multi-Status")
PHP_HTTP_RESPONSE_CODE(208, "Already Reported")
PHP_HTTP_RESPONSE_CODE(226, "IM Used")
PHP_HTTP_RESPONSE_CODE(300, "Multiple Choices")
PHP_HTTP_RESPONSE_CODE(301, "Moved Permanently")
PHP_HTTP_RESPONSE_CODE(302, "Found")
PHP_HTTP_RESPONSE_CODE(303, "See Other")
PHP_HTTP_RESPONSE_CODE(304, "Not Modified")
PHP_HTTP_RESPONSE_CODE(305, "Use Proxy")
PHP_HTTP_RESPONSE_CODE(307, "Temporary Redirect")
PHP_HTTP_RESPONSE_CODE(308, "Permanent Redirect")
PHP_HTTP_RESPONSE_CODE(400, "Bad Request")
PHP_HTTP_RESPONSE_CODE(401, "Unauthorized")
PHP_HTTP_RESPONSE_CODE(402, "Payment Required")
PHP_HTTP_RESPONSE_CODE(403, "Forbidden")
PHP_HTTP_RESPONSE_CODE(404, "Not Found")
PHP_HTTP_RESPONSE_CODE(405, "Method Not Allowed")
PHP_HTTP_RESPONSE_CODE(406, "Not Acceptable")
PHP_HTTP_RESPONSE_CODE(407, "Proxy Authentication Required")
PHP_HTTP_RESPONSE_CODE(408, "Request Timeout")
PHP_HTTP_RESPONSE_CODE(409, "Conflict")
PHP_HTTP_RESPONSE_CODE(410, "Gone")
PHP_HTTP_RESPONSE_CODE(411, "Length Required")
PHP_HTTP_RESPONSE_CODE(412, "Precondition Failed")
PHP_HTTP_RESPONSE_CODE(413, "Request Entity Too Large")
PHP_HTTP_RESPONSE_CODE(414, "Request URI Too Long")
PHP_HTTP_RESPONSE_CODE(415, "Unsupported Media Type")
PHP_HTTP_RESPONSE_CODE(416, "Requested Range Not Satisfiable")
PHP_HTTP_RESPONSE_CODE(417, "Expectation Failed")
PHP_HTTP_RESPONSE_CODE(422, "Unprocessible Entity")
PHP_HTTP_RESPONSE_CODE(423, "Locked")
PHP_HTTP_RESPONSE_CODE(424, "Failed Dependency")
PHP_HTTP_RESPONSE_CODE(426, "Upgrade Required")
PHP_HTTP_RESPONSE_CODE(428, "Precondition Required")
PHP_HTTP_RESPONSE_CODE(429, "Too Many Requests")
PHP_HTTP_RESPONSE_CODE(431, "Request Header Fields Too Large")
PHP_HTTP_RESPONSE_CODE(500, "Internal Server Error")
PHP_HTTP_RESPONSE_CODE(501, "Not Implemented")
PHP_HTTP_RESPONSE_CODE(502, "Bad Gateway")
PHP_HTTP_RESPONSE_CODE(503, "Service Unavailable")
PHP_HTTP_RESPONSE_CODE(504, "Gateway Timeout")
PHP_HTTP_RESPONSE_CODE(505, "HTTP Version Not Supported")
PHP_HTTP_RESPONSE_CODE(506, "Variant Also Negotiates")
PHP_HTTP_RESPONSE_CODE(507, "Insufficient Storage")
PHP_HTTP_RESPONSE_CODE(508, "Loop Detected")
PHP_HTTP_RESPONSE_CODE(510, "Not Extended")
PHP_HTTP_RESPONSE_CODE(511, "Network Authentication Required")


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
