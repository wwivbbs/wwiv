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
declare -r FULLOS=$(uname -a)
declare -r ARCH=$(uname -m)
declare -r HERE=$(dirname $(realpath $0))
declare -r NINJA=$(which ninja)

echo "Debug Info [OS] - ${OS}"
echo "Debug Info [FULLOS] - ${FULLOS}"
echo "Debug Info [ARCH] - ${ARCH}"
echo "Debug Info [HERE] - ${HERE}"
echo "Debug Info [NINJA] - ${NINJA}"

if [ "${ARCH}" == "aarch64" ]; then
	echo "Configuring for ARM64"
	export VCPKG_FORCE_SYSTEM_BINARIES=1
fi


S_MODLIST=$(git submodule status)
if [ -z "$S_MODLIST" ]
then
      echo "No Submodules Required - OK"
      SUBSOK="true"
else
	SUBSOK="true"
	while IFS= read -r line; do
		S_MODNAME=$(echo "${line}" | awk -F ' ' '{print $2}')
		if [[ ${line} = -* ]]; then
			echo "Submodule [${S_MODNAME}] - Missing"
			SUBSOK="false"
		else
			echo "Submodule [${S_MODNAME}] - OK"
		fi
	done <<< "${S_MODLIST}"
	if ! [ "${SUBSOK}" == "true" ]; then
		echo "Required submodules not found: Aborting"
		echo "Please run \"git submodule update --init --recursive\" to pull required submodules."
		exit 1
	fi
fi

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
