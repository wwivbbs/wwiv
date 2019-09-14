#!/bin/bash
declare -r OS=$(uname)

if [[ "${OS}" == "SunOS" ]]; then
    echo "Setting compiler to gcc for SunOS"
    export CXX=/usr/bin/g++
    export CC=/usr/bin/gcc
fi

declare -r NINJA=$(which ninja)
if [[ -x "${NINJA}" ]]; then
	echo "Using Ninja Build Tool: ${NINJA}"
else
	echo "** ERROR: Please install ninja"
	echo ""
	echo "   Debian: apt install ninja-build"
	echo "   CentOS: yum install ninja-build"
	exit 1
fi

cmake -DCMAKE_BUILD_TYPE:STRING=Debug -G "Ninja" $@
