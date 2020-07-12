#!/bin/bash
#
# Daily cleanup jobs
#

source /etc/wwiv/config

cd ${WWIV_DIR}

# BBS daily maintenance
echo "$(date '+%Y/%m/%d %H:%M:%S'): Running BBS daily Maintenance"
./bbs -e

# Put other daily events here (TW2002, for example)
