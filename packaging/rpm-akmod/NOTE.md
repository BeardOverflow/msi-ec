If you have secure boot on Fedora Silverblue/Kinoite, you have to use the following repo: https://github.com/CheariX/silverblue-akmods-keys to be able to load the module at boot.

rpm-ostree install \
  packaging/rpm-akmod/rpmbuild/RPMS/noarch/msi-ec-kmod-common-0.13-1.fc43.noarch.rpm \
  packaging/rpm-akmod/rpmbuild/RPMS/x86_64/akmod-msi-ec-0.13-1.fc43.x86_64.rpm
