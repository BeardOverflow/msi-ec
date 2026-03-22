#!/bin/bash
SRC_DIR=$(ls -d /usr/src/msi-ec-* | sort -V | tail -n 1)
VERSION_RELEASE=$(basename $SRC_DIR | sed 's/msi-ec-//')

# On slackware, we have to check manually for dkms
if command -v dkms >/dev/null 2>&1; then
    cd /usr/src/msi-ec-${VERSION_RELEASE} && make dkms-install
else
    echo "⚠️  DKMS not found. Driver source installed in /usr/src/ but not compiled."
fi