#!/bin/sh
#
# Sample script to call global war.
#
# Usage: gw.sh INST_NUM INST_DIR
#
# Configuration in WWIV:
#
#    A) Description  : GW
#    B) Filename     : /wwiv/gw.sh %N %I
#    C) ACS          : user.sl >= 10
#    D) ANSI         : Optional
#    E) Exec Mode    : STDIO
#    F) Launch From  : BBS Root Directory
#    G) Local only   : No
#    H) Multi user   : No
#    I) Usage        : 11
#    J) Registered by: AVAILABLE
#    L) Pause after  : No
#

INST_NUM=$1
INST_DIR=$2

DOORFILE=$(realpath ${INST_DIR}/DOOR.BAT)
echo C:\BNU\BNU.COM >${DOORFILE}
echo E: >${DOORFILE}
echo cd GW >>${DOORFILE}
echo "WAR.EXE /W E:\CHAIN.TXT" >>${DOORFILE}

unix2dos ${DOORFILE}

echo "num: ${INST_NUM} doors: ${INST_DIR}; DOORFILE: ${DOORFILE}"
./wwivqemu.sh ${INST_NUM} /wwiv/doors

rm ${DOORFILE}

