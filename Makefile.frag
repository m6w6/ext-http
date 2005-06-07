phpincludedir=$(prefix)/include/php

HTTP_HEADER_FILES= \
	phpstr/phpstr.h \
	php_http_cache_api.h \
	php_http_headers_api.h \
	php_http_response_object.h \
	php_http_util_object.h \
	php_http.h \
	php_http_request_api.h \
	php_http_message_api.h \
	php_http_send_api.h \
	php_http_api.h \
	php_http_date_api.h \
	php_http_message_object.h \
	php_http_std_defs.h \
	php_http_auth_api.h \
	php_http_exception_object.h \
	php_http_request_object.h \
	php_http_requestpool_object.h \
	php_http_url_api.h

install-http-headers:
	@echo "Installing HTTP headers:          $(INSTALL_ROOT)$(phpincludedir)/ext/http/"
	@$(mkinstalldirs) $(INSTALL_ROOT)$(phpincludedir)/ext/http
	@for f in $(HTTP_HEADER_FILES); do \
		if test -f "$(top_srcdir)/$$f"; then \
			$(INSTALL_DATA) $(top_srcdir)/$$f $(INSTALL_ROOT)$(phpincludedir)/ext/http; \
		elif test -f "$(top_builddir)/$$f"; then \
			$(INSTALL_DATA) $(top_builddir)/$$f $(INSTALL_ROOT)$(phpincludedir)/ext/http; \
		elif test -f "$(top_srcdir)/ext/http/$$f"; then \
			$(INSTALL_DATA) $(top_srcdir)/ext/http/$$f $(INSTALL_ROOT)$(phpincludedir)/ext/http; \
		elif test -f "$(top_builddir)/ext/http/$$f"; then \
			$(INSTALL_DATA) $(top_builddir)/ext/http/$$f $(INSTALL_ROOT)$(phpincludedir)/ext/http; \
		else \
			echo "WTF? $f $$f"; \
		fi \
	done;

# mini hack
install: $(all_targets) $(install_targets) install-http-headers

