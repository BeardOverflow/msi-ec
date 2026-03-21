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

package-all: deb alpine slackware

deb:
	@echo "📦 Building Debian package v$(VERSION)-$(RELEASE)..."
	nfpm pkg --target $(MODNAME)-dkms_$(VERSION)-$(RELEASE).deb

alpine:
	@echo "📦 Building Alpine package v$(VERSION)-$(RELEASE)..."
	nfpm pkg --target $(MODNAME)-dkms-$(VERSION)-$(RELEASE).apk

slackware:
	@echo "📦 Building Slackware (TXZ)..."
	nfpm pkg --packager archlinux --target $(MODNAME)-dkms-$(VERSION)-$(RELEASE).tar.xz
	mv $(MODNAME)-dkms-$(VERSION)-$(RELEASE).tar.xz $(MODNAME)-dkms-$(VERSION)-$(RELEASE).txz

clean-pkg:
	@echo "🧹 Cleaning packages..."
	rm -f *.deb *.apk *.txz

publish: package-all
	@if [ -z "$(CLOUDSMITH_API_KEY)" ]; then \
		echo "❌ Error: CLOUDSMITH API KEY not defined."; \
		exit 1; \
	fi
	@echo "🚀 Pushing Cloudsmith..."
	cloudsmith push deb $(CS_TARGET) $(MODNAME)-dkms_$(VERSION)-$(RELEASE).deb
	cloudsmith push alpine msi-ec/msi-ec/alpine/any-version $(MODNAME)-dkms-$(VERSION)-$(RELEASE).apk
	cloudsmith push raw $(CS_TARGET) $(MODNAME)-dkms-$(VERSION)-$(RELEASE).txz