include ../../Makefile.vars

export MODNAME
export VERSION

DATE_SLUG := $(shell date +%Y%m%d%H%M)

GIT_SLUG=$(shell git rev-parse --short HEAD)
export RELEASE := $(DATE_SLUG)git$(GIT_SLUG)

CS_ORG = msi-ec
CS_REPO = msi-ec
# Using any-distro/any-version because DKMS is universal.
CS_TARGET = $(CS_ORG)/$(CS_REPO)/any-distro/any-version

.PHONY: package-all deb slackware alpine clean-pkg

package-all: prepare-build deb alpine slackware

prepare-build:
	@sed -e "s|@MODNAME@|$(MODNAME)|g" \
	     -e "s|@VERSION@|$(VERSION)|g" \
	     -e "s|@RELEASE@|$(RELEASE)|g" \
	     -e "s|@VERSION_FULL@|$(FULL_VERSION)|g" \
	     scripts/postinstall.sh.in > scripts/postinstall.sh
	
	@sed -e "s|@MODNAME@|$(MODNAME)|g" \
	     -e "s|@VERSION@|$(VERSION)|g" \
	     -e "s|@RELEASE@|$(RELEASE)|g" \
	     -e "s|@VERSION_FULL@|$(FULL_VERSION)|g" \
	     scripts/postinstall_slackware.sh.in > scripts/postinstall_slackware.sh

	@sed -e "s|@MODNAME@|$(MODNAME)|g" \
	     -e "s|@VERSION@|$(VERSION)|g" \
	     -e "s|@RELEASE@|$(RELEASE)|g" \
	     -e "s|@VERSION_FULL@|$(FULL_VERSION)|g" \
	     scripts/preremove.sh.in > scripts/preremove.sh
	
	@chmod +x scripts/postinstall.sh scripts/preremove.sh scripts/postinstall_slackware.sh

deb: prepare-build
	@echo "📦 Building Debian package v$(VERSION)-$(RELEASE)..."
	nfpm pkg --target $(MODNAME)-dkms_$(VERSION)-$(RELEASE).deb

alpine: prepare-build
	@echo "📦 Building Alpine package v$(VERSION)-$(RELEASE)..."
	nfpm pkg --target $(MODNAME)-dkms-$(VERSION)-$(RELEASE).apk

slackware: prepare-build
	@echo "📦 Building Slackware (TXZ)..."
	nfpm pkg --packager archlinux --target $(MODNAME)-dkms-$(VERSION)-$(RELEASE).tar.xz
	mv $(MODNAME)-dkms-$(VERSION)-$(RELEASE).tar.xz $(MODNAME)-dkms-$(VERSION)-$(RELEASE).txz

clean-pkg:
	@echo "🧹 Cleaning packages..."
	rm -f *.deb *.apk *.txz scripts/postinstall_slackware.sh scripts/postinstall.sh scripts/preremove.sh

publish: package-all
	@if [ -z "$(CLOUDSMITH_API_KEY)" ]; then \
		echo "❌ Error: CLOUDSMITH API KEY not defined."; \
		exit 1; \
	fi
	@echo "🚀 Pushing Cloudsmith..."
	cloudsmith push deb $(CS_TARGET) $(MODNAME)-dkms_$(VERSION)-$(RELEASE).deb
	cloudsmith push alpine msi-ec/msi-ec/alpine/any-version $(MODNAME)-dkms-$(VERSION)-$(RELEASE).apk
	cloudsmith push raw msi-ec/msi-ec $(MODNAME)-dkms-$(VERSION)-$(RELEASE).txz