#!/bin/bash
# Expected usage is door.sh %C %H
# Or some other drop file for %C
set -x
source /etc/wwiv/config
source ${WWIV_DIR}/lib/door-lib-v0.sh

declare -r DIR=$(dirname $(readlink -f $0))

dd_set_door_dir ${DIR}
dd_set_dropfile $1
dd_set_sockethandle $2

echo "Door DIR:       ${DOOR_DIR}"
echo "Dropfile Path:  ${DROPFILE_PATH}"
echo "Temporary Path: ${BBS_TEMPDIR}"

# Note that it's gw.bat and not GW.BAT, it also
# doesn't include a drive letter nor path. This needs
# to match the UNIX name, dosemu will find it and
# execute it.
dd_dosemu gw.bat
