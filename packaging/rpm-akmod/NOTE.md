If you have secure boot on Fedora Silverblue/Kinoite, you have to use the following repo: https://github.com/CheariX/silverblue-akmods-keys to be able to load the module at boot.

sudo rpm-ostree install \
    packaging/rpm-akmod/rpmbuild/RPMS/noarch/msi-ec-kmod-common-*.rpm \
    packaging/rpm-akmod/rpmbuild/RPMS/x86_64/akmod-msi-ec-*.rpm
