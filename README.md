# ext-http

[![Build Status](https://travis-ci.org/m6w6/ext-http.svg?branch=master)](https://travis-ci.org/m6w6/ext-http)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/8711/badge.svg)](https://scan.coverity.com/projects/m6w6-ext-http)
[![codecov](https://codecov.io/gh/m6w6/ext-http/branch/master/graph/badge.svg)](https://codecov.io/gh/m6w6/ext-http)
[![Join the chat at https://gitter.im/m6w6/ext-http](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/m6w6/ext-http)

Extended HTTP support. Again.

## Documentation

See the [online markdown reference](https://mdref.m6w6.name/http).

Known issues are listed in [BUGS](./BUGS) and future ideas can be found in [TODO](./TODO).

## Installing

### PECL

	pecl install pecl_http

### PHARext

Watch out for [PECL replicates](https://replicator.pharext.org?pecl_http)
and pharext packages attached to [releases](https://github.com/m6w6/ext-http/releases).

### Checkout

	git clone github.com:m6w6/ext-http
	cd ext-http
	/path/to/phpize
	./configure --with-php-config=/path/to/php-config
	make
	sudo make install

## ChangeLog

A comprehensive list of changes can be obtained from the [ChangeLog](./CHANGELOG.md) and the list of [fixed CVEs](./CVE.md).

## License

ext-http is licensed under the 2-Clause-BSD license, which can be found in
the accompanying [LICENSE](./LICENSE) file.

## Contributing

All forms of contribution are welcome! Please see the bundled
[CONTRIBUTING](./CONTRIBUTING.md) note for the general principles followed.

The list of past and current contributors is maintained in [THANKS](./THANKS).
