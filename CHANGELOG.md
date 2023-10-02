# ChangeLog v4

## 4.2.4, 2023-10-02

* Fix Error using ssl array in options : Could not set option tlsauthtype
  (see  gh issue #131)
* Fix arginfo wargnings of the internal curl client user handler 
* Disable libidn support for v1.36-v1.38 due to broken locale detection

## 4.2.3, 2022-06-10

* Fix http\Client::requeue() not updating response callback

## 4.2.2, 2022-02-25

* Fixed gh-issue #123: Segfault with libcurl 7.81

## 4.2.1, 2021-09-13

* Fixed failing tests with PHP-8.1 (see gh issue #120)
* Fixed configure reliably finding the right libcurl features available
* Fixed cookie handling with libcurl 7.77+ and consistently across all 
  supported libcurl versions (follow-up to gh issue #116)

## 4.2.0, 2021-08-30

* Fixed PHP-8.1 compatibility (see gh issues #114, #115 and #118)
* Fixed cookies failing with libcurl >= 7.77 (see gh issue #116)
* Fixed tests using $_ENV instead of getenv() to find executables in PATH (see gh issue #113)
* Added http\Env::reset(): resets internal HTTP request cache (see gh issue #90)

## 4.1.0, 2021-04-21

* Added request options:
  * http\Client\Curl::$abstract_unix_socket
  * http\Client\Curl::$altsvc
  * http\Client\Curl::$altsvc_ctrl
  * http\Client\Curl::$aws_sigv4
  * http\Client\Curl::$doh_url
  * http\Client\Curl::$dns_shuffle_addresses
  * http\Client\Curl::$haproxy_protocol
  * http\Client\Curl::$hsts
  * http\Client\Curl::$hsts_ctrl
  * http\Client\Curl::$http09_allowed
  * http\Client\Curl::$maxage_conn
  * http\Client\Curl::$pinned_publickey
  * http\Client\Curl::$proxy_ssl
  * http\Client\Curl::$socks5_auth
  * http\Client\Curl::$tcp_fastopen
  * http\Client\Curl::$tls13_ciphers
  * http\Client\Curl::$xoauth2_bearer
* Added request option constants:
  * http\Client\Curl\AUTH_AWS_SIGV4
  * http\Client\Curl\AUTH_BEARER
  * http\Client\Curl\AUTH_NONE
  * http\Client\Curl\HTTP_VERSION_2_PRIOR_KNOWLEDGE
  * http\Client\Curl\HTTP_VERSION_3
  * http\Client\Curl\SSL_VERSION_MAX_*
  * http\Client\Curl\SSL_VERSION_TLSv1_3
* Added library version constants:
  * http\Client\Curl\Versions\BROTLI
  * http\Client\Curl\Versions\CAINFO
  * http\Client\Curl\Versions\CAPATH
  * http\Client\Curl\Versions\HYPER
  * http\Client\Curl\Versions\ICONV
  * http\Client\Curl\Versions\NGHTTP2
  * http\Client\Curl\Versions\QUIC
  * http\Client\Curl\Versions\ZSTD
 
## 4.0.0, 2021-01-13

Changes from beta1:
* Fixed configure on systems which do not provide icu-config
* Fixed gh-issue #89: Cookie handling cannot be disabled since v3.2.1

## 4.0.0beta1, 2020-09-23

* PHP 8 compatibility
	- Drop ext-propro support:  
		PHP 8 removes the object get/set API from the ZendEngine, which renders
		that extension dysfunctional. As a consequence, the header property of
		http\Message and derived classes cannot be modified in place, and thus
		by reference.
