#!/bin/bash

FILTER="msi-ec|msi_ec"

sudo dkms status | grep -E "$FILTER" | while read -r line; do
    
    module_info=$(echo "$line" | cut -d',' -f1)
    
    module_name=$(echo "$module_info" | cut -d'/' -f1)
    module_version=$(echo "$module_info" | cut -d'/' -f2)

    if [ -n "$module_name" ] && [ -n "$module_version" ]; then
        sudo dkms remove "$module_name/$module_version" --all
    fi
done