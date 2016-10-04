# ChangeLog v2

## 2.6.0RC1, 2016-10-04

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
* Fixed PHP-5.3 compatibility
* Fixed recursive calls to the event loop dispatcher

Changes from beta2:
* Fix bug #73055: crash in http\QueryString (Mike, @rc0r) (CVE-2016-7398)
* Fix bug #73185: Buffer overflow in HTTP parse_hostinfo() (Mike, @rc0r)
* Fix HTTP/2 version parser for older libcurl versions (Mike)

## 2.6.0beta2, 2016-09-07

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
* Fixed PHP-5.3 compatibility
* Fixed recursive calls to the event loop dispatcher

## 2.6.0beta1, 2016-08-22

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

## 2.5.6, 2016-03-09

* Fix php-bug #71719: Buffer overflow in HTTP url parsing functions (Mike, rc0r)
* Fix gh-issue #28: Possible null pointer dereference in php_http_url_mod() (rc0r)
* Fix gh-issue #22: Fix PHP5 config.w32 (Jan Ehrhardt)
* Fix gh-issue #20: setSslOptions notice with curl 7.43 (Mike, Vitaliy Demidov)

## 2.5.5, 2015-12-07

* Fixed gh-issue #16: No Content-Length header with empty POST requests

## 2.5.3, 2015-09-25

* Fixed gh-issue #12: crash on bad url passed to http\Message::setRequestUrl()
* The URL parser now fails on empty labels

## 2.5.2, 2015-09-10

* Fixed regression with HEAD requests always warning about a partial file transfer
+ Added "path_as_is" request option (libcurl >= 7.42)

## 2.5.1, 2015-07-28

* Fixed VC11 build (Jan Erhardt)
* Fixed gh-issue #2: comparison of obsolete pointers in the header parser (xiaoyjy)
* Fixed gh-issue #6: allow RFC1738 unsafe characters in query/fragment
* Fixed gh-issue #7: crash with querystring and exception from error handler
+ SSL certinfo is available for libcurl >= 7.42 with gnutls (openssl has already been since 7.19.1)
+ Added "falsestart" SSL request option (available with libcurl >= 7.42 and darwinssl/NSS)
+ Added "service_name" and "proxy_service_name" request options for SPNEGO (available with libcurl >= 7.43)
+ Enabled "certinfo" transfer info on all supporting SSL backends (OpenSSL: libcurl v7.19.1, NSS: libcurl v7.34.0, GSKit: libcurl v7.39.0, GnuTLS: libcurl v7.42.0)

## 2.5.0, 2015-07-09

+ Added RFC5988 (Web Linking) support to http\Params
+ Added http\Url::SANITIZE_PATH to default flags of http\Url::mod()
* Fixed overly aggressive response caching to only consider 2xx cachable

## 2.5.0RC1, 2015-05-21

* Added RFC5988 (Web Linking) support to http\Params
* Added http\Url::SANITIZE_PATH to default flags of http\Url::mod()
* Fixed overly aggressive response chaching to only consider 2xx cachable

## 2.4.3, 2015-04-08

* Fixed bug #69357 (HTTP/1.1 100 Continue overriding subsequent 200 response code with PUT request)

## 2.4.2, 2015-04-03

* Fixed bug 69076 (http\Url throws Exception on empty querystring) (p at wspnr dot com)
* Fixed bug 69313 (http\Client doesn't send GET body)
+ Added libidn2 and UIDNA as fallbacks for IDN support
- Deferred warnings/exceptions of the client, so callbacks for the currently failing requests will still be called

## 2.4.1, 2015-03-18

* Fixed build with PHP <= 5.4 (Remi)

## 2.4.0, 2015-03-18

* Split off pecl/apfd and pecl/json_post

## 2.3.2, 2015-03-12

* Fixed bug with http\QueryString::offsetSet() resetting the complete query string

## 2.3.1, 2015-03-02

* Fixed build on platforms that need stddef.h to define ptrdiff_t (e.g. CentOS 7.5)

## 2.3.0, 2015-03-01

+ Preliminiary HTTP2 support for http\Client (libcurl with nghttp2 support)
+ Improved performance of HTTP info parser (request/response line)
+ Improved performance of updating client observers
+ Improved performance of http\Env\Response output to streams
+ Improved the error messages of the header parser
+ Added http\Header\Parser class
+ Added http\Client::configure() method accepting an array with the following options for libcurl:
  . maxconnects (int, size of the connection cache)
  . max_host_connections (int, max number of connections to a single host, libcurl >= 7.30.0)
  . max_pipeline_length (int, max number of requests in a pipeline, libcurl >= 7.30.0)
  . max_total_connections (int, max number of simultaneous open connections of this client, libcurl >= 7.30.0)
  . pipelining (bool, whether to enable HTTP/1.1 pipelining)
  . chunk_length_penalty_size (int, chunk length threshold for pipelining, libcurl >= 7.30.0)
  . content_length_penalty_size (int, size threshold for pipelining, libcurl >= 7.30.0)
  . pipelining_server_bl (array, list of server software names to blacklist for pipelining, libcurl >= 7.30.0)
  . pipelining_site_bl (array, list of server host names to blacklist for pipelining, libcurl >= 7.30.0)
  . use_eventloop (bool, whether to use libevent, libcurl+libevent)
+ Added http\Client::getAvailableOptions() and http\Client::getAvailableConfiguration() methods
+ Added support for HTTP2 if libcurl was built with nghttp2 support.
+ Added http\Client\Curl\HTTP_VERSION_2_0 constant (libcurl >= 7.33.0)
+ Added http\Client\Curl\TLS_AUTH_SRP constant (libcurl >= 7.21.4)
+ Added pinned_publickey SSL request option (libcurl >= 7.39.0)
+ Added tlsauthtype, tlsauthuser and tlsauthpass SSL request option (libcurl >= 7.21.4)
+ Added verifystatus (a.k.a OCSP) SSL request option (libcurl >= 7.41.0)
+ Added proxyheader request option (libcurl >= 7.37.0)
+ Added unix_socket_path request option (libcurl >= 7.40.0)
* Fixed compress request option
* Fixed parsing authorities of CONNECT messages
* Fixed parsing Content-Range messages
* Fixed http\Env\Response to default to chunked encoding over streams
* Fixed superfluous output of Content-Length:0 headers
* Fixed persistent easy handles to be only created for persistent multi handles
* Fixed the header parser to accept not-yet-complete header lines
* Fixed http\Message::toStream() crash in ZTS mode
* Fixed the message stream parser to handle intermediary data bigger than 4k
* Fixed the message stream parser to handle single header lines without EOL
* Fixed http\Message\Body to not generate stat based etags for temporary streams
- Deprecated http\Client::enablePipelining(), use http\Client::configure(["pipelining" => true]) instead
- Deprecated http\Client::enableEvents(), use http\Client::configure(["use_eventloop" => true]) instead
- Removed the cookies entry from the transfer info, wich was very slow and generated a Netscape formatted list of cookies
- Changed the header parser to reject illegal characters

Changes from RC1:
* Fixed a shutdown crash with chunked encoded stream responses

## 2.3.0RC1, 2015-02-19

+ Preliminiary HTTP2 support for http\Client (libcurl with nghttp2 support)
+ Improved performance of HTTP info parser (request/response line)
+ Improved performance of updating client observers
+ Improved performance of http\Env\Response output to streams
+ Improved the error messages of the header parser
+ Added http\Header\Parser class
+ Added http\Client::configure() method accepting an array with the following options for libcurl:
  . maxconnects (int, size of the connection cache)
  . max_host_connections (int, max number of connections to a single host, libcurl >= 7.30.0)
  . max_pipeline_length (int, max number of requests in a pipeline, libcurl >= 7.30.0)
  . max_total_connections (int, max number of simultaneous open connections of this client, libcurl >= 7.30.0)
  . pipelining (bool, whether to enable HTTP/1.1 pipelining)
  . chunk_length_penalty_size (int, chunk length threshold for pipelining, libcurl >= 7.30.0)
  . content_length_penalty_size (int, size threshold for pipelining, libcurl >= 7.30.0)
  . pipelining_server_bl (array, list of server software names to blacklist for pipelining, libcurl >= 7.30.0)
  . pipelining_site_bl (array, list of server host names to blacklist for pipelining, libcurl >= 7.30.0)
  . use_eventloop (bool, whether to use libevent, libcurl+libevent)
+ Added http\Client::getAvailableOptions() and http\Client::getAvailableConfiguration() methods
+ Added support for HTTP2 if libcurl was built with nghttp2 support.
+ Added http\Client\Curl\HTTP_VERSION_2_0 constant (libcurl >= 7.33.0)
+ Added http\Client\Curl\TLS_AUTH_SRP constant (libcurl >= 7.21.4)
+ Added pinned_publickey SSL request option (libcurl >= 7.39.0)
+ Added tlsauthtype, tlsauthuser and tlsauthpass SSL request option (libcurl >= 7.21.4)
+ Added verifystatus (a.k.a OCSP) SSL request option (libcurl >= 7.41.0)
+ Added proxyheader request option (libcurl >= 7.37.0)
+ Added unix_socket_path request option (libcurl >= 7.40.0)
* Fixed compress request option
* Fixed parsing authorities of CONNECT messages
* Fixed parsing Content-Range messages
* Fixed http\Env\Response to default to chunked encoding over streams
* Fixed superfluous output of Content-Length:0 headers
* Fixed persistent easy handles to be only created for persistent multi handles
* Fixed the header parser to accept not-yet-complete header lines
* Fixed http\Message::toStream() crash in ZTS mode
* Fixed the message stream parser to handle intermediary data bigger than 4k
* Fixed the message stream parser to handle single header lines without EOL
* Fixed http\Message\Body to not generate stat based etags for temporary streams
- Deprecated http\Client::enablePipelining(), use http\Client::configure(["pipelining" => true]) instead
- Deprecated http\Client::enableEvents(), use http\Client::configure(["use_eventloop" => true]) instead
- Removed the cookies entry from the transfer info, wich was very slow and generated a Netscape formatted list of cookies
- Changed the header parser to reject illegal characters

## 2.2.1, 2015-02-07

* Fixed Bug #69000 (http\Url breaks down with very long URL query strings)

## 2.2.0, 2015-01-26

- var_dump(http\Message) no longer automatically creates an empty body
+ Added http\Message\Parser class
+ Made http\Client::once() and http\Client::wait() available when using events
+ Added http\Url::PARSE_MBLOC, http\Url::PARSE_MBUTF8, http\Url::PARSE_TOIDN and http\Url::PARSE_TOPCT constants
+ Added http\Env\Response::setCookie()
+ Added http\Env\Request::getCookie()

## 2.2.0RC1, 2014-11-12

- var_dump(http\Message) no longer automatically creates an empty body
+ Added http\Message\Parser class
+ Made http\Client::once() and http\Client::wait() available when using events
+ Added http\Url::PARSE_MBLOC, http\Url::PARSE_MBUTF8, http\Url::PARSE_TOIDN and http\Url::PARSE_TOPCT constants
+ Added http\Env\Response::setCookie()
+ Added http\Env\Request::getCookie()

## 2.1.4, 2014-11-06

* Fixed bug #68353 (QsoSSL support removed in libcurl 7.39)
* Fixed bug #68149 (duplicate content-length with libcurl < 7.23)
* Fixed bug #66891 (Unexpected HTTP 401 after NTLM authentication)

## 2.1.3, 2014-10-16

* Fix build with libcurl < 7.26 (Remi)

## 2.1.2, 2014-09-25

+ Added missing request option constants:
  POSTREDIR_303, AUTH_SPNEGO (libcurl >= 7.38.0), SSL_VERSION_TLSv1_{0,1,2} (libcurl >= 7.34)
* Fixed bug #68083 (PUT method not working after DELETE)
* Fixed bug #68009 (Segmentation fault after calling exit(0) after a request)
* Fixed bug #68000 (Extension does not build on FreeBSD)

## 2.1.1, 2014-09-09

* Fix httpVersion retrieval on bigendian (Remi)
* Fix etag/crc32b on bigendian (Remi)

## 2.1.0, 2014-09-01

- Removed port and scheme guessing of http\Url for portability
* Fixed PHP-5.3 compatibility
* Fixed PHP-5.4 compatibility
* Fixed possible bus error on shutdown when using events
* Fixed sovereignty of clients when using events
* Fixed a possible crash with http\Encoding\Stream\Dechunk::decode($unencoded)
* Fixed a leak in http\Client\Curl options
* Fixed bug #67733 (Compile error with libevent 2.x)
+ Added RFC5987 support in http\Params
+ Improved synthetic HTTP message parsing performace for ~20%
+ Added request options if libcurl has builtin c-ares support:
  dns_interface, dns_local_ip4, dns_local_ip6 (all libcurl >= 7.33.0)
+ Added request options:
  expect_100_timeout (libcurl >= 7.36.0), tcp_nodelay
+ Added transfer info:
  curlcode, tls_session (libcurl >= 7.34.0), only available during transfer

## 2.1.0RC3, 2014-08-19

Changes from RC2:
* Fixed PHP-5.3 compatibility
* Fixed possible bus error on shutdown when using events
+ Added curlcode transfer info
- Removed port and scheme guessing of http\Url for portability

## 2.1.0RC2, 2014-08-05

Changes from RC1:
* Fixed a possible crash with http\Encoding\Stream\Dechunk::decode($unencoded)
* Fixed a leak in http\Client\Curl options
* Fixed PHP-5.4 compatibility

## 2.1.0RC1, 2014-08-01

* Fixed bug #67733 (Compile error with libevent 2.x)
+ Added RFC5987 support in http\Params
+ Improved synthetic HTTP message parsing performace for ~20%
+ Added request options if libcurl has builtin c-ares support:
  dns_interface, dns_local_ip4, dns_local_ip6 (all libcurl >= 7.33.0)
+ Added request options:
  expect_100_timeout (libcurl >= 7.36.0)
  tcp_nodelay
+ Added transfer info:
  tls_session (libcurl >= 7.34.0), only available during transfer

## 2.0.7, 2014-07-11

* General improvements to the test suite
* Fixed http\Env\Response::send() ignoring some write errors
* Fixed bug #67528 (RFC compliant default user agent)
* Fixed a garbage collector issue with JSON POSTs
* Fixed refcount issue and double free of message bodies
* Fixed use after free if the http\Client::enqueue() closure returns TRUE
* Fixed bug #67584 (http\Client\Response not initialized as response on failure)

## 2.0.6, 2014-04-24

+ Added "uploaded" progress state
* Fixed bug #67089 (Segmentaion fault with ZTS)
* Fixed compatibility with PHP-5.6+
* Fixed re-use of request messages which content length remained untouched when the body was reset

## 2.0.5, 2014-04-04

* Fix rare crash with uninitialized CURLOPT_HTTPHEADER
* Fix build with -Werror=format-security (Remi)
* Fix build with extenal libs needed by libcurl

## 2.0.4, 2014-01-02

* Removed the pecl/event conflict
* Fixed bug #66388 (Crash on POST with Content-Length:0 and untouched body)

## 2.0.3, 2013-12-10

* Fixed typo

## 2.0.2, 2013-12-10

* Fixed bug #66250 (shutdown crash as shared extension)

## 2.0.1, 2013-11-26

* Fixed a bug with multiple ob_start(http\Env\Response) while replacing the body
* Fixed build on Windows with libevent2

## 2.0.0, 2013-11-22

Extended HTTP support. Again.

Keep in mind that it's got the major version 2, because it's incompatible with pecl_http v1.

## 2.0.0beta5, 2013-08-12

Extended HTTP support. Again. Keep in mind that it's got the major version 2, because it's incompatible with pecl_http v1.

* Introduces the http namespace.
* Message bodies have been remodeled to use PHP temporary streams instead of in-memory buffers.
* The utterly misunderstood HttpResponse class has been reimplemented as http\Env\Response inheriting http\Message.
* Currently, there's only one Exception class left, http\Exception.
* Errors triggered by the extension can be configured statically by http\Object::$defaultErrorHandling or inherited http\Object->errorHandling.
* The request ecosystem has been modularized to support different libraries, though for the moment only libcurl is supported.

## 2.0.0beta4, 2012-12-31

! >80% test coverage http://goo.gl/VmyIW
* Fixed build with libcurl <= 7.21.3
* Fixed var_dump of http\Message with inherited userland properties with increased access level
+ Added http\Header::getParams()
+ Added simple support for escapes and quotes in the params parser
+ Added support for sending http\Env\Response over PHP streams
+ Added message body reference counting

## 2.0.0beta3, 2012-12-13

! >80% test coverage http://goo.gl/YCV74
* Fixed http\Env\Response throttling
* Fixed http\Env\Response caching by last-modified
* Fixed http\Message::addBody()
* Fixed http\Message::parentMessage write access
* Fixed crash with freed but not nulled event_base pointer
* Fixed crash with null pointer dereference on http\Encoding\Stream::flush()
* Fixed some memory leaks
+ Added http\Header::negotiate()
+ Added http\Header::parse()

## 2.0.0beta2, 2012-11-29

! >80% test coverage
* Fixed http\Request\Pool with libevent2
* Fixed http\Env\Request::getFiles() with multiple-file-uploads
* Fixed PHP-5.3 compatibility
* Fixed reference handling of http\Message\Body::getResource()
* Fixed reading stream filters to correctly detect EOF of tmp and mem streams
- Change: merge message headers with the same key
- Change: the stream message parser can optionally return after each message
- Change: you have to care yourself for Content headers if a message's body has a reading stream filter attached
+ Added http\Env::getResponseStatusForAllCodes()

## 2.0.0beta1, 2012-10-11

* PHP-5.3 compatibility by Anatoly Belsky
* Fixed http\Client's history handling
* Disallow serialization of non-serializable objects
* Fixed parsing of folded headers
* Fixed the parsing HTTP messages from streams
* Fixed leak in persistent handles cleanup routine
+ Added http\Url::SANITIZE_PATH; URL paths are not sanitized by default anymore
+ Added JSON Content-Type handler for request body processing if ext/json is present
+ Added missing IANA HTTP response codes
+ Added http\Message\Body::getResource()
+ Added QueryString proxy methods to http\Env\Request
+ Added Serializable to http\Message\Body's interfaces

## 2.0.0alpha1, 2012-04-13

+ Added http\Client\AbstractClient::request(string method, string url[, array headers=null[, mixed body=null[, array options=null]]])
+ Added constants http\Params::PARSE_RAW, ::PARSE_DEFAULT, ::PARSE_URLENCODED, ::PARSE_DIMENSION, ::PARSE_QUERY
+ Added fourth parameter 'flags' to http\Params' constructor, which defaults to http\Params::PARSE_DEFAULT
* Fixed bug #61444 (query string converts . to _ in param names)

## 2.0.0dev10, 2012-03-30

+ This release contains the http\Request to http\Client refactoring triggered by Benjamin Eberlei. Many thanks.

## 2.0.0dev9, 2012-03-23

+ Added population of $_POST and $_FILES for non-POST requests
- Renamed http\Env\Request::getPost() to ::getForm()
- Changed http\Env\Response::setContentDisposition() to take an http\Params like array as argument
- Removed http\Env\Response::CONTENT_DISPOSOTION_* constants
- Removed http\Request\Method class; request methods are now used as simple strings

## 2.0.0dev8, 2012-03-16

* Fixed build failure and compiler warnings
* Fixed logical errors in http\Env\Response::isCachedBy{Etag,LastModified}()
* Fixed memory leaks in http\Env\Response::isCachedByLastModified()
* Fixed memory leaks in http\Env::getResponseHeader()
* Fixed erroneous trailing CRLF of http\Message strings
- Renamed http\Message\Body::add() to ::addForm()
+ Added http\Message\Body::addPart(http\Message $part)
+ Added http\Env\Response::__invoke() output buffering handler

## 2.0.0dev7, 2012-03-09

+ Added multipart support to http\Message, which can now splitMultipartBody()
  to a http\Message chain, f.e. of a ranges response or file upload request.
+ Added primitive quoting/escaping capabilities to http\Params.
+ Reworked and improved negotiation support, added asterisk (*) matching etc.

## 2.0.0dev6, 2012-03-01

+ Added stream parsing capability to http\Message
+ Added http\Env\Request methods: getQuery(), getPost(), getFiles()
* Changed http\Env\Response to only cache responses to GET or HEAD requests without authorization
* Fixed possible crash when http\Url was initialized with empty urls

## 2.0.0dev5, 2012-02-17

* Improved test coverage [1] and fixed a lot of issues with the cookie, params, querystring,
  persistent handles, request factory, etag, stream filters, encoding streams, negotiation
  and HTTP message info code.

[1] http://dev.iworks.at/ext-http/lcov/ext/http/index.html

## 2.0.0dev4, 2012-01-23

This is to become v2 of the known pecl_http extension.
It is completely incompatible to previous version.
Try it, or let it be. If you are not sure, let it be. Really.

List of changes (TBD):
* Everything lives below the http namespace
* The message body is implemented as a temp stream instead of a chunk of memory
* The utterly misunderstood HttpResponse class has been reimplemented in the http\env namespace
* There's only http\Exception
* Every instance follows http\Object::$defaultErrorHandling or inherited http\Object->errorHandling, but only for errors generated by the extension itself
* You have to use the http\Request\Factory to create your requests/pools/datashares

## 2.0.0dev3, 2012-01-16

This is to become v2 of the known pecl_http extension.
It is completely incompatible to previous version.
Try it, or let it be. If you are not sure, let it be. Really.

List of changes (TBD):
* Everything lives below the http namespace
* The message body is implemented as a temp stream instead of a chunk of memory
* The utterly misunderstood HttpResponse class has been reimplemented in the http\env namespace
* There's only http\Exception
* Every instance follows http\Object::$defaultErrorHandling or inherited http\Object->errorHandling, but only for errors generated by the extension itself
* You have to use the http\Request\Factory to create your requests/pools/datashares

## 2.0.0dev2, 2011-06-14

This is to become v2 of the known pecl_http extension.
It is completely incompatible to previous version.
Try it, or let it be. If you are not sure, let it be. Really.

List of changes (TBD):
* Everything lives below the http namespace
* Supported request libraries: curl, neon
* The message body is implemented as a temp stream instead of a chunk of memory
* The utterly misunderstood HttpResponse class has been reimplemented in the http\env namespace
* There's only http\Exception
* Every instance follows http\Object::$defaultErrorHandling or inherited http\Object->errorHandling, but only for errors generated by the extension itself

## 2.0.0dev1, 2011-06-02

This is to become v2 of the known pecl_http extension.
It is completely incompatible to previous version.
Try it, or let it be. If you are not sure, let it be. Really.

List of changes (TBD):
* Everything lives below the http namespace
* Supported request libraries: curl, neon
* The message body is implemented as a temp stream instead of a chunk of memory
* The utterly misunderstood HttpResponse class has been reimplemented in the http\env namespace
* There's only http\Exception
* Every instance follows http\Object::$defaultErrorHandling or inherited http\Object->errorHandling, but only for errors generated by the extension itself
