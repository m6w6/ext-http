# vim: noet ts=1 sw=1

phpincludedir=$(prefix)/include/php

install-http: install install-http-headers

install-http-headers:
	@echo "Installing HTTP headers:          $(INSTALL_ROOT)$(phpincludedir)/ext/http/"
	@$(mkinstalldirs) $(INSTALL_ROOT)$(phpincludedir)/ext/http
	@for f in $(PHP_HTTP_HEADERS); do \
		if test -f "$(top_srcdir)/$$f"; then \
			$(INSTALL_DATA) $(top_srcdir)/$$f $(INSTALL_ROOT)$(phpincludedir)/ext/http; \
		elif test -f "$(top_builddir)/$$f"; then \
			$(INSTALL_DATA) $(top_builddir)/$$f $(INSTALL_ROOT)$(phpincludedir)/ext/http; \
		elif test -f "$(top_srcdir)/ext/http/$$f"; then \
			$(INSTALL_DATA) $(top_srcdir)/ext/http/$$f $(INSTALL_ROOT)$(phpincludedir)/ext/http; \
		elif test -f "$(top_builddir)/ext/http/$$f"; then \
			$(INSTALL_DATA) $(top_builddir)/ext/http/$$f $(INSTALL_ROOT)$(phpincludedir)/ext/http; \
		else \
			echo "WTF? $$f"; \
		fi \
	done;

