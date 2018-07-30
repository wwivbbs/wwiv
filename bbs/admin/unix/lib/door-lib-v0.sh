#!/bin/bash
############################################
# UNIX Door Shell Script Library
# - A collection of helpers using DOSEMU
#
# Example Door Script:
#
#   !/bin/bash
#   source /bbs/doors/door-lib-v0.sh
#
#   Expected usage is gw.sh %C %H
#
#   dd_set_door_dir /bbs/doors/gw
#   dd_set_dropfile $1
#   dd_set_sockethandle $2
#   # Note that it's gw.bat and not GW.BAT, it also
#   # doesn't include a drive letter nor path. This needs
#   # to match the UNIX name, dosemu will find it and
#   # execute it.
#dd_dosemu gw.bat



echo "Starting door..."

DROPFILE_PATH=""
DROPFILE_NAME=""
BBS_TEMPDIR=""
DOOR_DIR=""
SOCKET_HANDLE=-1

if [[ -z "${DOSEMU_RC}" ]]; then
    echo "Setting DOSEMU_RC"
    DOSEMU_RC=${HOME}/.dosemurc
fi

##############################################
#
# die with message.
#
die() { echo "$*" 1>&2 ; exit 1; }


##############################################
#
# Test pre-requisites
#

function check_in_path() {
    if ! [[ -x "$(command -v $1)" ]]; then
	die "$1 must be installed"
    fi
}

check_in_path dos2unix
check_in_path dosemu

##############################################
#
# Sets the full path to the dropfile.
#
# $1 is the full path to the dropdile
function dd_set_dropfile() {
    DROPFILE_PATH=$1
    DROPFILE_NAME=$(basename ${DROPFILE_PATH})
    if [[ -z "${BBS_TEMPDIR}" ]]; then
	# Set the BBS_TEMPDIR to the directory for the dropfile
	# If we haven't already set it.
	BBS_TEMPDIR=$(dirname ${DROPFILE_PATH})
    fi
}

##############################################
#
# Sets the full path to the temp/node
# directory
#
# $1 = tempdir
function dd_set_temp_dir() {
    BBS_TEMPDIR=$1
}


is_num() { [[ ${1} =~ ^[0-9]+$ ]]; }

##############################################
#
# Set the socket handle.
#
# $1 = socket handle
#
function dd_set_sockethandle() {
    if ! [[ ${1} =~ ^[0-9]+$ ]]; then
	die "dd_set_sockethandle expects a number, got $1"
    fi
    SOCKET_HANDLE=$1
}

# $1 is door dir
function dd_set_door_dir() {
    DOOR_DIR=$1
}

##############################################
#
# Sets the environment variables needed for
# the DOS directories baeds on the door_dir
# and temp_dir set (either directly or as the
# directory for the dropfile.
#
function dosemu_paths() {
    # D: is door path
    # T: is TEMP path
    export DOSDRIVE_D=${DOOR_DIR}
    export DOSDRIVE_T=${BBS_TEMPDIR}
}

##############################################
#
# Executes DOSEMU on command passed as $1
# $1 is the name of the command to execute.
#
# Note that it's gw.bat and not GW.BAT, it also
# doesn't include a drive letter nor path. This needs
# to match the UNIX name, dosemu will find it and
# execute it.
#
function dd_dosemu() {
    if [[ -z "$1" ]]; then
	die "execute_dosemu needs the command specified as a param"
    fi

    if [[ -z "${DROPFILE_PATH}" ]]; then
	die "The path to the drop file must be specified to dd_set_dropfile"
    fi

    # Ensure that the dropfile is in DOS format.
    unix2dos ${DROPFILE_PATH}
    
    if [[ -z "${DOSDRIVE_D}" || -z "${DOSDRIVE_T}" ]]; then
	dosemu_paths D T
    fi
    local bat=$1
    cd ${DOOR_DIR}
    socat -d -d \
	  FD:${SOCKET_HANDLE} \
	  EXEC:"dosemu -f ${DOSEMU_RC} ${bat}",pty
}

