# Makefile specific to COPR.
srpm:
	# Installing deps (just in case)
	dnf install -y rpmdevtools rpmspectool
	cp ../../Makefile.vars . || true
	ls
	# Call packaging makefile
	$(MAKE) -C ../packaging/rpm-akmod/ srpm outdir=$(outdir)