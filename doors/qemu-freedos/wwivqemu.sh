#!/bin/sh
#
# QEMU Launcher for WWIV Doors.
#
# This assumes the following drives for the FreeDOS session:
# C: boot disk from freedos.img
# D: Doors root directory (Linux dir containing all doors)
# E: Temp/Instance directory for the BBS.
#
# Usage:
#
# - write out DOOR.BAT to the temp/instance dir with the command to run.
# q.sh NODE_NUM DOORS_DIR
#

# customize this to match your install
WWIV_TEMP=/wwiv/e


INSTANCE_NUM="${1:-1}"
DOORS="${2:-/wwiv/doors}"
INST=${WWIV_TEMP}/${INSTANCE_NUM}/temp
BASE_BOOT_IMG=/wwiv/freedos.img
BOOT_IMG=${INST}/$(basename ${BASE_BOOT_IMG})

cp ${BASE_BOOT_IMG} ${BOOT_IMG}

export TERM=linux 
qemu-system-x86_64 \
    -drive format=raw,file=${BOOT_IMG} \
    -drive format=raw,file=fat:rw:${DOORS} \
    -drive format=raw,file=fat:rw:${INST} \
    -chardev stdio,id=char0,signal=off \
    -serial chardev:char0 \
    -display none \
    -monitor none \
    -nographic

