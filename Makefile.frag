# provide headers in builddir, so they do not end up in /usr/include/ext/http/src

PHP_HTTP_HEADERS := $(addprefix $(PHP_HTTP_BUILDDIR)/,$(PHP_HTTP_HEADERS))

$(PHP_HTTP_BUILDDIR)/%.h: $(PHP_HTTP_SRCDIR)/src/%.h
	@cat >$@ <$<

$(all_targets): http-build-headers
clean: http-clean-headers

.PHONY: http-build-headers
http-build-headers: $(PHP_HTTP_HEADERS)

.PHONY: http-clean-headers
http-clean-headers:
	-rm -f $(PHP_HTTP_HEADERS)
