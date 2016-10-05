# CVE

List of [CVE](http://cve.mitre.org/)s regarding pecl/http.

ID            | GH-Issue/PHP-Bug                                   | Summary                                       | Fixed in           | Commit
--------------|----------------------------------------------------|-----------------------------------------------|--------------------|-------
CVE-2016-5873 | [PHP-71719](https://bugs.php.net/bug.php?id=71719) | Buffer overflow in HTTP url parsing functions | 2.5.6, 3.0.1 | https://github.com/m6w6/ext-http/commit/3724cd76a28be1d6049b5537232e97ac567ae1f5
CVE-2016-7398 | [PHP-73055](https://bugs.php.net/bug.php?id=73055) | Type confusion vulnerability in merge_param() | 2.6.0RC1, 3.1.0RC1 | https://github.com/m6w6/ext-http/commit/17137d4ab1ce81a2cee0fae842340a344ef3da83
CVE-2016-7961 | [PHP-73185](https://bugs.php.net/bug.php?id=73185) | Buffer overflow in HTTP parse_hostinfo()      | 2.6.0RC1, 3.1.0RC1 | https://github.com/m6w6/ext-http/commit/ec043079e9915d7d1f4cb06eeadb2c7fca195658