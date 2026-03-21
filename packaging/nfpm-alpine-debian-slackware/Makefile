include ../../Makefile.vars

DATE_RPM=$(date +"%a %b %d %Y")

GIT_SLUG=$(shell git rev-parse --short HEAD)
export RELEASE := $(DATE_SLUG)git$(GIT_HASH)

CS_ORG = msi-ec
CS_REPO = msi-ec
# Using any-distro/any-version because DKMS is universal.
CS_TARGET = $(CS_ORG)/$(CS_REPO)/any-distro/any-version

.PHONY: package-all deb slackware alpine clean-pkg

package-all: deb slackware alpine

deb:
	@echo "📦 Building Debian package v$(VERSION)-$(RELEASE)..."
	nfpm pkg --target msi-ec_$(VERSION)-$(RELEASE).deb

alpine:
	@echo "📦 Building Alpine package v$(VERSION)-$(RELEASE)..."
	nfpm pkg --target msi-ec-$(VERSION)-$(RELEASE).apk

slackware:
	@echo "📦 Building Slackware (TXZ)..."
	nfpm pkg --target msi-ec-$(VERSION)-$(RELEASE).txz

clean-pkg:
	@echo "🧹 Cleaning packages..."
	rm -f *.deb *.apk *.txz

publish: package-all
	@if [ -z "$(CLOUDSMITH_API_KEY)" ]; then \
		echo "❌ Error: CLOUDSMITH API KEY not defined."; \
		exit 1; \
	fi
	@echo "🚀 Pushing Cloudsmith..."
	cloudsmith push deb $(CS_TARGET) msi-ec_$(VERSION)-$(RELEASE).deb
	cloudsmith push alpine $(CS_TARGET) msi-ec-$(VERSION)-$(RELEASE).apk
	cloudsmith push slackware $(CS_TARGET) msi-ec-$(VERSION)-$(RELEASE).txz