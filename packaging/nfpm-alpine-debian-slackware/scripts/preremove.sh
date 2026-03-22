#!/bin/bash
SRC_DIR=$(ls -d /usr/src/msi-ec-* | sort -V | tail -n 1)
VERSION_RELEASE=$(basename $SRC_DIR | sed 's/msi-ec-//')

cd /usr/src/msi-ec-${VERSION_RELEASE}
make dkms-uninstall