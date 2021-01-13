# ext-http

[![Build Status](https://github.com/m6w6/ext-http/workflows/ci/badge.svg?branch=master)](https://github.com/m6w6/ext-http/actions?query=workflow%3Aci+branch%3Amaster)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/8711/badge.svg)](https://scan.coverity.com/projects/m6w6-ext-http)
[![codecov](https://codecov.io/gh/m6w6/ext-http/branch/master/graph/badge.svg)](https://codecov.io/gh/m6w6/ext-http)

Extended HTTP support for PHP.

## Branches and Versions:

> **NOTE:**  
  Use `v3.x` branch, and resp. v3 releases, for PHP-7. `master` and v4 releases are only for PHP-8.


## Documentation

See the [online markdown reference](https://mdref.m6w6.name/http).

Known issues are listed in [BUGS](./BUGS) and future ideas can be found in [TODO](./TODO).

## Install

### PECL

	pecl install pecl_http

### PHARext

Watch out for [PECL replicates](https://replicator.pharext.org?pecl_http)
and pharext packages attached to [releases](https://github.com/m6w6/ext-http/releases).

### Checkout

	git clone https://github.com/m6w6/ext-http.git
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
