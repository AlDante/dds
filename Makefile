.PHONY: docs profile profile-clean

docs:
	$(MAKE) -C docs html

profile:
	$(MAKE) -C src macos_profile
	$(MAKE) -C test -f Makefiles/Makefile_Mac_clang profile_binaries

profile-clean:
	rm -rf src/build-profile test/build-profile

