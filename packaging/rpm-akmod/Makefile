-include ../../Makefile.vars
-include Makefile.vars

PWD        := $(shell pwd)
RPM_ROOT   := $(PWD)/rpmbuild
DIST_DIR   := $(MODNAME)-$(VERSION)

DATE_RPM=$(date +"%a %b %d %Y")

GIT_SLUG=$(shell git rev-parse --short HEAD)
TIMESTAMP=$(shell date +%Y%m%d%H%M)
NEW_RELEASE="1.${TIMESTAMP}git${GIT_SLUG}"

.PHONY: srpm clean dist

clean:
	rm -rf $(RPM_ROOT) $(DIST_DIR).tar.gz

dist: clean
	mkdir -p $(DIST_DIR)
	sed -i 's|CURDIR|PWD|g' ../../Makefile
	cp ../../{*.c,*.h,Makefile,Makefile.vars,LICENSE} $(DIST_DIR)/
	tar -czf $(DIST_DIR).tar.gz $(DIST_DIR)
	rm -rf $(DIST_DIR)

srpm: dist
	mkdir -p $(RPM_ROOT)/{SOURCES,SPECS,SRPMS}
	mv $(DIST_DIR).tar.gz $(RPM_ROOT)/SOURCES/
	
	echo "$(MODNAME)" > $(RPM_ROOT)/SOURCES/module-load.conf
	
	cp depmod-override.conf $(RPM_ROOT)/SOURCES/ ; \
	
	sed -e "s|@MODNAME@|$(MODNAME)|g" \
	    -e "s|@VERSION@|$(VERSION)|g" \
	    -e "s|@DATE@|$(DATE)|g" \
	    -e "s|@USER@|$(USER_NAME)|g" \
	    -e "s|@EMAIL@|$(USER_EMAIL)|g" \
		-e "s|@RELEASE@|${NEW_RELEASE}|g" \
	    akmod.spec.in > $(RPM_ROOT)/SPECS/$(MODNAME)-kmod.spec

	git log -n 7 --pretty=format:"* %ad GitHub Actions <no-reply@github.com> - ${VERSION}-${NEW_RELEASE}%n- %s (%h)%n" --date=format:"%a %b %d %Y" >> $(RPM_ROOT)/SPECS/$(MODNAME)-kmod.spec

	rpmbuild --define "_topdir $(RPM_ROOT)" -bs $(RPM_ROOT)/SPECS/$(MODNAME)-kmod.spec
	@if [ -n "$(outdir)" ]; then cp $(RPM_ROOT)/SRPMS/*.src.rpm $(outdir)/; fi