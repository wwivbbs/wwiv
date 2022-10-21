#!/bin/bash

# WWIV Version 5.x
# Copyright (C)2014-2022, WWIV Software Services
#
# Licensed  under the  Apache License, Version  2.0 (the "License");
# you may not use this  file  except in compliance with the License.
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless  required  by  applicable  law  or agreed to  in  writing,
# software  distributed  under  the  License  is  distributed on an
# AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,
# either  express  or implied.  See  the  License for  the specific
# language governing permissions and limitations under the License.
#

declare -r OS=$(uname)
declare -r HERE=$(dirname $(realpath $0))
declare -r NINJA=$(which ninja)

if ! [ -x "$(command -v cmake)" ]; then
	echo "** ERROR: Please install cmake"
	exit 1
fi

if ! [ -x "$(command -v ninja)" ]; then
	echo "** ERROR: Please install ninja"
	echo ""
	echo "   Debian/Ubuntu: apt install ninja-build"
	echo "   CentOS:        yum install ninja-build"
	exit 1
fi

if [[ "${OS}" == "SunOS" ]]; then
    echo "Setting compiler to gcc for SunOS"
    export CXX=/usr/bin/g++
    export CC=/usr/bin/gcc
fi

echo "Using Ninja Build Tool: ${NINJA}"
echo "Using Source Root:      ${HERE}"

cmake -G "Ninja" ${HERE}  -DCMAKE_TOOLCHAIN_FILE=${HERE}/vcpkg/scripts/buildsystems/vcpkg.cmake "${@}"
