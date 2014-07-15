#!/bin/bash

while true; do
    nwm $@
    [ $? -ne 100 ] && break
    git sync
    if [ -f build.ninja ]; then 
        ninja 
    else
        make
    fi
    echo "Restarting nwm..."
done
