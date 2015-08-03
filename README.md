# pecl/http v2

[![Build Status](https://travis-ci.org/m6w6/ext-http.svg?branch=master)](https://travis-ci.org/m6w6/ext-http)

## About:

Extended HTTP support. Again. 

* Introduces the http namespace.
* PHP stream based message bodies.
* Encapsulated env request/response.
* Modular client support.

## Installation:

This extension is hosted at [PECL](http://pecl.php.net) and can be installed with [PEAR](http://pear.php.net)'s pecl command:

    # pecl install pecl_http

## Dependencies:

pecl/http depends on a number of system libraries and PHP extensions for special features.

#### Required system libraries:

The following system libraries are required to build this extension:

##### zlib
Provides gzip/zlib/deflate encoding.  
Minimum version: 1.2.0.4  
Install on Debian: `apt-get install zlib1g-dev`


#### Optional system libraries:

The following system libraries are optional and provide additional features:

##### libidn
Provides IDNA support in URLs.  
Minimum version: none  
Install on Debian: `apt-get install libidn11-dev`

##### libidn2
Provides IDNA support in URLs (fallback if libidn is not available).  
Minimum version: none  
Install on Debian: `apt-get install libidn2-0-dev`

##### libicu
Provides IDNA support in URLs (fallback if libidn is not available).  
Minimum version: none  
Install on Debian: `apt-get install libicu-dev`

##### libcurl
Provides HTTP request functionality.  
Minimum version: 7.18.2  
Install on Debian: `apt-get install libcurl4-openssl-dev`  
Note: There are usually different styles of SSL support for libcurl available, so you can replace 'openssl' in the above command f.e. with 'nss' or 'gnutls'.

##### libevent
Eventloop support for the HTTP client.  
Minimum version: none  
Install on Debian: `apt-get install libevent-dev`

### PHP extensions:

This extension unconditionally depends on the pre-loaded presence of the following PHP extensions:

* [raphf](https://github.com/m6w6/ext-raphf)
* [propro](https://github.com/m6w6/ext-propro)
* spl


If configured ```--with-http-shared-deps``` (default) it depends on the pre-loaded presence of the following extensions, as long as they were available at build time:

* hash
* iconv
* json (only until < 2.4.0)

Please ensure that all extension on which pecl/http depends, are loaded before it, e.g in your `php.ini`:

	; obligatory deps
	extension = raphf.so
	extension = propro.so
	
	; if shared deps were enabled
	extension = hash.so
	extension = iconv.so
	extension = json.so
	
	; finally load pecl/http
	extension = http.so

## Conflicts:

pecl/http-v2 conflicts with the following extensions:

* http-v1
* event (only until <= 2.0.3)

## INI Directives:

* http.etag.mode = "crc32b"  
  Default hash method for dynamic response payloads to generate an ETag.

## Stream Filters:

The http extension registers the ```http.*``` namespace for its stream filters. Provided stream filters are:

* http.chunked_decode  
  Decode a stream encoded with chunked transfer encoding.
* http.chunked_encode  
  Encode a stream with chunked transfer encoding.
* http.inflate  
  Decode a stream encoded with deflate/zlib/gzip encoding.
* http.deflate  
  Encode a stream with deflate/zlib/gzip encoding.


## Documentation:

Documentation is available at http://devel-m6w6.rhcloud.com/mdref/http/
