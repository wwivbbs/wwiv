#!/bin/bash
set -e

if [[ $(id -u) -ne 0 ]]; then
   echo "$0 must be executed as root."
   exit 1
fi

echo "Installing prerequistes for Debian/Ubuntu"

apt-get update
apt-get install -y libncurses5-dev build-essential git cmake zip ninja-build gettext zlib1g-dev
# gettext is in there since some versions of git don't express that dep properly.
