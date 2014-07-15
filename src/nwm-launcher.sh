#!/bin/bash

while true; do
    nwm $@
    [ $? -ne 100 ] && break
    git stash
    git pull
    git stash pop
    if [ -f build.ninja ]; then 
        ninja 
    else
        make
    fi
    echo "Restarting nwm..."
done
