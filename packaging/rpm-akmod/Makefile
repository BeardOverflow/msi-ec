MODNAME := msi-ec
VERSION := $(shell grep "^VERSION" ../../Makefile | head -1 | cut -d'=' -f2 | xargs)
RPM_ROOT := $(shell pwd)/rpmbuild

USER_NAME  := MSI-EC Project
USER_EMAIL := https://github.com/BeardOverflow/msi-ec
DATE_SPEC  := $(shell date "+%a %b %d %Y")

all:
	@echo "Usage: make rpm-local | make rpm-dist | make clean"

rpm-local: clean
	@echo "--- Preparing local build for Fedora (Hacked) ---"
	mkdir -p $(RPM_ROOT)/{SOURCES,SPECS,RPMS,SRPMS}
	
	@echo "--- Creating sources tarball ---"
	tar --exclude='../../packaging' --exclude='../../.git' \
		--exclude='../../.github' --exclude='../../docs' \
		--transform 's|^.*/|$(MODNAME)-$(VERSION)/|' \
		--transform 's|^\.|$(MODNAME)-$(VERSION)|' \
		-czf $(RPM_ROOT)/SOURCES/$(MODNAME)-$(VERSION).tar.gz ../../
	
	@echo "--- Generating spec file ---"
	sed -e "s|@MODNAME@|$(MODNAME)|g" \
	    -e "s|@VERSION@|$(VERSION)|g" \
	    -e "s|@DATE@|$(DATE_SPEC)|g" \
	    -e "s|@USER@|$(MAINTAINER_NAME)|g" \
	    -e "s|@EMAIL@|$(MAINTAINER_EMAIL)|g" \
	    akmod.spec.in > $(RPM_ROOT)/SPECS/akmod-$(MODNAME).spec
	
	@echo "--- Building RPM with Local Hacks ---"
	rpmbuild --define "_topdir $(RPM_ROOT)" \
		--define "kmod_repo fedora" \
		--define "akmod_install 1" \
		--define "build_kernels akmod" \
		-ba $(RPM_ROOT)/SPECS/akmod-$(MODNAME).spec

rpm-dist: clean
	@echo "--- Preparing clean SRPM for RPM Fusion ---"
	mkdir -p $(RPM_ROOT)/{SOURCES,SPECS,RPMS,SRPMS}
	
	tar --exclude='../../packaging' --exclude='../../.git' \
		--transform 's|^\.\./\.\./|$(MODNAME)-$(VERSION)/|' \
		-czf $(RPM_ROOT)/SOURCES/$(MODNAME)-$(VERSION).tar.gz ../../
	
	sed -e "s|@MODNAME@|$(MODNAME)|g" \
	    -e "s|@VERSION@|$(VERSION)|g" \
	    -e "s|@DATE@|$(DATE_SPEC)|g" \
	    -e "s|@USER@|$(MAINTAINER_NAME)|g" \
	    -e "s|@EMAIL@|$(MAINTAINER_EMAIL)|g" \
	    akmod.spec.in > $(RPM_ROOT)/SPECS/akmod-$(MODNAME).spec
	
	rpmbuild --define "_topdir $(RPM_ROOT)" -ba $(RPM_ROOT)/SPECS/akmod-$(MODNAME).spec

clean:
	rm -rf $(RPM_ROOT)

.PHONY: all rpm-local rpm-dist clean