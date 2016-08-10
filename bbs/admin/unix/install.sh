#!/bin/bash
#
# install.sh
#
# Runs through the basic steps to install.  We do make certain assumptions
# in order to facilitate the setup.  In particular, we assume we are running
# from the install wwivbase location.

WWIV_DIR=`pwd`
LOGFILE=install_`date "+%Y-%m-%d_%H-%M-%S"`.log

echo
echo "Starting the install process"
echo "Starting the install process" > ${WWIV_DIR}/$LOGFILE 2>&1
echo

# Make sure init exists
if [ ! -f ${WWIV_DIR}/init ]
then
    echo "init is missing.  Please compile your BBS files and copy them"
    echo "init is missing.  Please compile your BBS files and copy them" >> ${WWIV_DIR}/$LOGFILE 2>&1
    echo "to ${WWIV_DIR} and run install.sh again."
    echo "to ${WWIV_DIR} and run install.sh again." >> ${WWIV_DIR}/$LOGFILE 2>&1
    exit 1
fi

# configure scripts and helper binaries.
echo "Configuring system scripts"
echo "Configuring system scripts" >> ${WWIV_DIR}/$LOGFILE 2>&1
echo
echo "Setting scripts to use your install location ${WWIV_DIR}"
echo "Setting scripts to use your install location ${WWIV_DIR}" >> ${WWIV_DIR}/$LOGFILE 2>&1
for i in bin/processmail.sh bin/daily-cleanup.sh .wwivrc in.nodemgr wwiv-service
do
    sed -i "s@REPLACE-WWIVBASE@${WWIV_DIR}@" $i
done

echo
echo "Your BBS basic data setup is complete."
echo "running ./init now to finalize the BBS.  "
sleep 5

./init

echo
echo "init complete.  Please run ./bbs and hit the spacebar to login"
echo "and set up a new user to be the sysop (#1) account"
echo
