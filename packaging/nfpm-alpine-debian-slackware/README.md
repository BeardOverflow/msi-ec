# NFPM
NFPM is a tool that allow generating packages for multiple distributions. However, it is not suitable in our case for RHEL/Fedora based distro as they banned dkms usage.
This yaml file will allow dynamically building packages for:
 - Alpine based distro
 - Debian based distro
 - Arch based distro

The scripts folder contains scripts that will be embedded to packages.