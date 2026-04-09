DOXYGEN ?= doxygen
DOXYFILE := Doxyfile
.PHONY: html clean
html:
	@command -v "$(DOXYGEN)" >/dev/null 2>&1 || { \
		echo "doxygen not found on PATH"; \
		exit 1; \
	}
	$(DOXYGEN) $(DOXYFILE)
clean:
	rm -rf build
