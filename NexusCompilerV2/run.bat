#!/bin/bash

if [ -z "$1" ]; then
    echo "no param or doesn't exist"
    exit
fi

cd build && cmake --build . && cd .. && ./build/NexusCompiler $1
