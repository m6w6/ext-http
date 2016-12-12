# ChangeLog v3

## 3.1.0, 2016-12-12

+ Added http\Client\Curl\User interface for userland event loops
+ Added http\Url::IGNORE_ERRORS, http\Url::SILENT_ERRORS and http\Url::STDFLAGS
+ Added http\Client::setDebug(callable $debug)
+ Added http\Client\Curl\FEATURES constants and namespace
+ Added http\Client\Curl\VERSIONS constants and namespace
+ Added share_cookies and share_ssl (libcurl >= 7.23.0) options to http\Client::configure()
+ http\Client uses curl_share handles to properly share cookies and SSL/TLS sessions between requests
+ Improved configure checks for default CA bundles
+ Improved negotiation precision
* Fixed regression introduced by http\Params::PARSE_RFC5987: negotiation using the params parser would receive param keys without the trailing asterisk, stripped by http\Params::PARSE_RFC5987.
* Fix gh-issue #50: http\Client::dequeue() within http\Client::setDebug() causes segfault (Mike, Maik Wagner)
* Fix gh-issue #47: http\Url: Null pointer deref in sanitize_value() (Mike, @rc0r)
* Fix gh-issue #45: HTTP/2 response message parsing broken with libcurl >= 7.49.1 (Mike)
* Fix gh-issue #43: Joining query with empty original variable in query (Mike, Sander Backus)
* Fix gh-issue #42: fatal error when using punycode in URLs (Mike, Sebastian Thielen)
* Fix gh-issue #41: Use curl_version_info_data.features when initializing options (Mike)
* Fix gh-issue #40: determinde the SSL backend used by curl at runtime (Mike, @rcanavan)
* Fix gh-issue #39: Notice: http\Client::enqueue(): Could not set option proxy_service_name (Mike, @rcanavan)
* Fix gh-issue #38: Persistent curl handles: error code not properly reset (Mike, @afflerbach)
* Fix gh-issue #36: Unexpected cookies sent if persistent_handle_id is used (Mike, @rcanavan, @afflerbach)
* Fix gh-issue #34: allow setting multiple headers with the same name (Mike, @rcanavan)
* Fix gh-issue #33: allow setting prodyhost request option to NULL (Mike, @rcanavan)
* Fix gh-issue #31: add/improve configure checks for default CA bundle/path (Mike, @rcanavan)

Changes from beta1:
* Fixed recursive calls to the event loop dispatcher

Changes from beta2:
+ Improved configure checks for IDNA libraries (added --with-http-libicu-dir, --with-http-libidnkit{,2}-dir, --with-http-libidn2-dir)
* Fix bug #73055: crash in http\QueryString (Mike, @rc0r) (CVE-2016-7398)
* Fix bug #73185: Buffer overflow in HTTP parse_hostinfo() (Mike, @rc0r) (CVE-2016-7961)
* Fix HTTP/2 version parser for older libcurl versions (Mike)
* Fix gh-issue #52: Underscores in host names: libidn Failed to parse IDN (Mike, @canavan)

## 3.1.0RC1, 2016-10-04

+ Added http\Client\Curl\User interface for userland event loops
+ Added http\Url::IGNORE_ERRORS, http\Url::SILENT_ERRORS and http\Url::STDFLAGS
+ Added http\Client::setDebug(callable $debug)
+ Added http\Client\Curl\FEATURES constants and namespace
+ Added http\Client\Curl\VERSIONS constants and namespace
+ Added share_cookies and share_ssl (libcurl >= 7.23.0) options to http\Client::configure()
+ http\Client uses curl_share handles to properly share cookies and SSL/TLS sessions between requests
+ Improved configure checks for default CA bundles
+ Improved negotiation precision
* Fixed regression introduced by http\Params::PARSE_RFC5987: negotiation using the params parser would receive param keys without the trailing asterisk, stripped by http\Params::PARSE_RFC5987.
* Fix gh-issue #50: http\Client::dequeue() within http\Client::setDebug() causes segfault (Mike, Maik Wagner)
* Fix gh-issue #47: http\Url: Null pointer deref in sanitize_value() (Mike, @rc0r)
* Fix gh-issue #45: HTTP/2 response message parsing broken with libcurl >= 7.49.1 (Mike)
* Fix gh-issue #43: Joining query with empty original variable in query (Mike, Sander Backus)
* Fix gh-issue #42: fatal error when using punycode in URLs (Mike, Sebastian Thielen)
* Fix gh-issue #41: Use curl_version_info_data.features when initializing options (Mike)
* Fix gh-issue #40: determinde the SSL backend used by curl at runtime (Mike, @rcanavan)
* Fix gh-issue #39: Notice: http\Client::enqueue(): Could not set option proxy_service_name (Mike, @rcanavan)
* Fix gh-issue #38: Persistent curl handles: error code not properly reset (Mike, @afflerbach)
* Fix gh-issue #36: Unexpected cookies sent if persistent_handle_id is used (Mike, @rcanavan, @afflerbach)
* Fix gh-issue #34: allow setting multiple headers with the same name (Mike, @rcanavan)
* Fix gh-issue #33: allow setting prodyhost request option to NULL (Mike, @rcanavan)
* Fix gh-issue #31: add/improve configure checks for default CA bundle/path (Mike, @rcanavan)

Changes from beta1:
* Fixed recursive calls to the event loop dispatcher

Changes from beta2:
+ Improved configure checks for IDNA libraries (added --with-http-libicu-dir, --with-http-libidnkit{,2}-dir, --with-http-libidn2-dir)
* Fix bug #73055: crash in http\QueryString (Mike, @rc0r) (CVE-2016-7398)
* Fix bug #73185: Buffer overflow in HTTP parse_hostinfo() (Mike, @rc0r) (CVE-2016-7961)
* Fix HTTP/2 version parser for older libcurl versions (Mike)
* Fix gh-issue #52: Underscores in host names: libidn Failed to parse IDN (Mike, @canavan)

## 3.1.0beta2, 2016-09-07

+ Added http\Client\Curl\User interface for userland event loops
+ Added http\Url::IGNORE_ERRORS, http\Url::SILENT_ERRORS and http\Url::STDFLAGS
+ Added http\Client::setDebug(callable $debug)
+ Added http\Client\Curl\FEATURES constants and namespace
+ Added http\Client\Curl\VERSIONS constants and namespace
+ Added share_cookies and share_ssl (libcurl >= 7.23.0) options to http\Client::configure()
+ http\Client uses curl_share handles to properly share cookies and SSL/TLS sessions between requests
+ Improved configure checks for default CA bundles
+ Improved negotiation precision
* Fixed regression introduced by http\Params::PARSE_RFC5987: negotiation using the params parser would receive param keys without the trailing asterisk, stripped by http\Params::PARSE_RFC5987.
* Fix gh-issue #50: http\Client::dequeue() within http\Client::setDebug() causes segfault (Mike, Maik Wagner)
* Fix gh-issue #47: http\Url: Null pointer deref in sanitize_value() (Mike, @rc0r)
* Fix gh-issue #45: HTTP/2 response message parsing broken with libcurl >= 7.49.1 (Mike)
* Fix gh-issue #43: Joining query with empty original variable in query (Mike, Sander Backus)
* Fix gh-issue #42: fatal error when using punycode in URLs (Mike, Sebastian Thielen)
* Fix gh-issue #41: Use curl_version_info_data.features when initializing options (Mike)
* Fix gh-issue #40: determinde the SSL backend used by curl at runtime (Mike, @rcanavan)
* Fix gh-issue #39: Notice: http\Client::enqueue(): Could not set option proxy_service_name (Mike, @rcanavan)
* Fix gh-issue #38: Persistent curl handles: error code not properly reset (Mike, @afflerbach)
* Fix gh-issue #36: Unexpected cookies sent if persistent_handle_id is used (Mike, @rcanavan, @afflerbach)
* Fix gh-issue #34: allow setting multiple headers with the same name (Mike, @rcanavan)
* Fix gh-issue #33: allow setting prodyhost request option to NULL (Mike, @rcanavan)
* Fix gh-issue #31: add/improve configure checks for default CA bundle/path (Mike, @rcanavan)

Changes from beta1:
* Fixed recursive calls to the event loop dispatcher

## 3.1.0beta1, 2016-08-22

+ Added http\Client\Curl\User interface for userland event loops
+ Added http\Url::IGNORE_ERRORS, http\Url::SILENT_ERRORS and http\Url::STDFLAGS
+ Added http\Client::setDebug(callable $debug)
+ Added http\Client\Curl\FEATURES constants and namespace
+ Added http\Client\Curl\VERSIONS constants and namespace
+ Added share_cookies and share_ssl (libcurl >= 7.23.0) options to http\Client::configure()
+ http\Client uses curl_share handles to properly share cookies and SSL/TLS sessions between requests
+ Improved configure checks for default CA bundles
+ Improved negotiation precision
* Fixed regression introduced by http\Params::PARSE_RFC5987: negotiation using the params parser would receive param keys without the trailing asterisk, stripped by http\Params::PARSE_RFC5987.
* Fix gh-issue #50: http\Client::dequeue() within http\Client::setDebug() causes segfault (Mike, Maik Wagner)
* Fix gh-issue #47: http\Url: Null pointer deref in sanitize_value() (Mike, @rc0r)
* Fix gh-issue #45: HTTP/2 response message parsing broken with libcurl >= 7.49.1 (Mike)
* Fix gh-issue #43: Joining query with empty original variable in query (Mike, Sander Backus)
* Fix gh-issue #42: fatal error when using punycode in URLs (Mike, Sebastian Thielen)
* Fix gh-issue #41: Use curl_version_info_data.features when initializing options (Mike)
* Fix gh-issue #40: determinde the SSL backend used by curl at runtime (Mike, @rcanavan)
* Fix gh-issue #39: Notice: http\Client::enqueue(): Could not set option proxy_service_name (Mike, @rcanavan)
* Fix gh-issue #38: Persistent curl handles: error code not properly reset (Mike, @afflerbach)
* Fix gh-issue #36: Unexpected cookies sent if persistent_handle_id is used (Mike, @rcanavan, @afflerbach)
* Fix gh-issue #34: allow setting multiple headers with the same name (Mike, @rcanavan)
* Fix gh-issue #33: allow setting prodyhost request option to NULL (Mike, @rcanavan)
* Fix gh-issue #31: add/improve configure checks for default CA bundle/path (Mike, @rcanavan)

## 3.0.1, 2016-03-09

* Fix php-bug #71719: Buffer overflow in HTTP url parsing functions (Mike, rc0r)
* Fix gh-issue #28: Possible null pointer dereference in php_http_url_mod() (rc0r)
* Fix gh-issue #21: Fix PHP7 config.w32 (Jan Ehrhardt)
* Fix gh-issue #20: setSslOptions notice with curl 7.43 (Mike, Vitaliy Demidov)

## 3.0.0, 2016-01-19

PHP7 compatible release based on the 2.5.x series with the following backwards incompatible changes:
- removed http\Url::FROM_ENV from the default flags of the http\Url constructor, use http\Env\Url instead

## 3.0.0RC1, 2015-12-07

PHP7 compatible release based on the 2.5.x series with the following backwards incompatible changes:
- removed http\Url::FROM_ENV from the default flags of the http\Url constructor, use http\Env\Url instead

