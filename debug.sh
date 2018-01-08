#!/bin/bash
declare -r OS=$(uname)
echo "$(pwd)"

if [[ "${OS}" == "SunOS" ]]; then
    echo "Setting compiler to gcc for SunOS"
    export CXX=/usr/bin/g++
    export CC=/usr/bin/gcc
fi

cmake -DCMAKE_BUILD_TYPE:STRING=Debug $@
