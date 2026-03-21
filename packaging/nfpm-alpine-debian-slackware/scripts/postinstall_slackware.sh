#!/bin/bash
# On slackware, we have to check manually for dkms
if command -v dkms >/dev/null 2>&1; then
    cd /usr/src/msi-ec-${VERSION} && make dkms-install
else
    echo "⚠️  DKMS not found. Driver source installed in /usr/src/ but not compiled."
fi