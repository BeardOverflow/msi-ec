include ../../Makefile.vars

PWD        := $(shell pwd)
RPM_ROOT   := $(PWD)/rpmbuild
DIST_DIR   := $(MODNAME)-$(VERSION)

USER_NAME  := $(shell git config user.name || echo "MSI-EC maintainers")
USER_EMAIL := $(shell git config user.email || echo "no-reply@msiecmaintainers.lan")
DATE       := $(shell date +'%a %b %d %Y')

.PHONY: srpm clean dist

clean:
	rm -rf $(RPM_ROOT) $(DIST_DIR).tar.gz

dist: clean
	mkdir -p $(DIST_DIR)
	cp ../../{*.c,*.h,Makefile,Makefile.vars} $(DIST_DIR)/
	tar -czf $(DIST_DIR).tar.gz $(DIST_DIR)
	rm -rf $(DIST_DIR)

srpm: dist
	mkdir -p $(RPM_ROOT)/{SOURCES,SPECS,SRPMS}
	mv $(DIST_DIR).tar.gz $(RPM_ROOT)/SOURCES/
	cp {module-load.conf,depmod-override.conf} $(RPM_ROOT)/SOURCES/
	sed -e "s|@MODNAME@|$(MODNAME)|g" \
	    -e "s|@VERSION@|$(VERSION)|g" \
	    -e "s|@DATE@|$(DATE)|g" \
	    -e "s|@USER@|$(USER_NAME)|g" \
	    -e "s|@EMAIL@|$(USER_EMAIL)|g" \
	    akmod.spec.in > $(RPM_ROOT)/SPECS/$(MODNAME)-kmod.spec
	rpmbuild --define "_topdir $(RPM_ROOT)" -bs $(RPM_ROOT)/SPECS/$(MODNAME)-kmod.spec
	@if [ -n "$(outdir)" ]; then cp $(RPM_ROOT)/SRPMS/*.src.rpm $(outdir)/; fi