# Makefile specific to COPR.
srpm:
	dnf install -y rpmdevtools rpmspectool
	$(MAKE) -C packaging/rpm-akmod/ srpm outdir=$(outdir)