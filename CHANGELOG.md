# ChangeLog v4

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
