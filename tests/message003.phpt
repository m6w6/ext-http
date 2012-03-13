--TEST--
multipart message
--SKIPIF--
<? include "skipif.inc";
--FILE--
<?
$m = new http\Message(fopen(__DIR__."/data/message_r_multipart_put.txt","rb"));
if ($m->isMultipart($boundary)) {
    var_dump($boundary);

    foreach ($m->splitMultipartBody() as $mm) {
        echo "===\n",$mm,"===\n";
    }
}
?>
DONE
--EXPECTF--
string(40) "----------------------------6e182425881c"
===
Content-Disposition: form-data; name="composer"; filename="composer.json"
Content-Type: application/octet-stream
Content-Length: 567

{
    "name": "mike_php_net/autocracy",
    "type": "library",
    "description": "http\\Controller preserves your autocracy",
    "keywords": ["http", "controller", "pecl", "pecl_http"],
    "homepage": "http://github.com/mike-php-net/autocracy",
    "license": "BSD-2",
    "authors": [
        {
            "name": "Michael Wallner",
            "email": "mike@php.net"
        }
    ],
    "require": {
        "php": ">=5.4.0",
        "pecl/pecl_http": "2.*"
    },
    "autoload": {
        "psr-0": {
            "http\\Controller": "lib"
        }
    }
}


===
===
Content-Disposition: form-data; name="LICENSE"; filename="LICENSE"
Content-Type: application/octet-stream
Content-Length: 1354

Copyright (c) 2011-2012, Michael Wallner <mike@iworks.at>.
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE 
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



===
DONE
