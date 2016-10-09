#!/bin/bash
#
# Daily cleanup jobs
#

source /etc/default/wwivd

cd ${WWIV_DIR}

# BBS daily maintenance
echo "$(date '+%Y/%m/%d %H:%M:%S'): Running BBS daily Maintenance"
./bbs -e

# Put other daily events here (TW2002, for example)
