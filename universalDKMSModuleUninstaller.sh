#!/bin/bash

FILTER="msi-ec|msi_ec"

dkms status | grep -E "$FILTER" | while read -r line; do
    
    module_info=$(echo "$line" | cut -d',' -f1)
    
    # module_info is either "name/version" (added) or "name/version: state"
    module_name=${module_info%%/*}
    module_version=${module_info#*/}
    module_version=${module_version%%:*}

    if [ -n "$module_name" ] && [ -n "$module_version" ] && sudo dkms status | grep -q "$module_name"; then
        dkms remove "$module_name/$module_version" --all
    fi
done