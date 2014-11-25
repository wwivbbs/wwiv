#!/bin/bash
#
# install.sh
#
# Runs through the basic steps to install.  We do make certain assumptions
# in order to facilitate the setup.  In particular, we assume we are running
# from the install wwivbase location.

WWIVBASE=`pwd`
LOGFILE=install_`date "+%Y-%m-%d_%H-%M-%S"`.log

echo
echo "Starting the install process"
echo "Starting the install process" > ${WWIVBASE}/$LOGFILE 2>&1

which unzip >> ${WWIVBASE}/$LOGFILE 2>&1
STATUS=$?

if [ "$STATUS" -ne "0" ]
then 
    echo "unzip utility is missing.  Please install it and run install again" 
    echo "unzip utility is missing.  Please install it and run install again" >> ${WWIVBASE}/$LOGFILE 2>&1
    exit 1
fi

echo
echo "Configuring data directories"
echo "Configuring data directories" >> ${WWIVBASE}/$LOGFILE 2>&1

# make directories
mkdir gfiles data >> ${WWIVBASE}/$LOGFILE 2>&1

# unzip menus
cd gfiles
unzip ../en-menus.zip >> ${WWIVBASE}/$LOGFILE 2>&1
cd ${WWIVBASE}

# unzip regions
cd data
unzip ../regions.zip >> ${WWIVBASE}/$LOGFILE 2>&1
cd ${WWIVBASE}

# unzip zip-city
cd data
unzip ../zip-city.zip >> ${WWIVBASE}/$LOGFILE 2>&1
cd ${WWIVBASE}

# configure scripts and helper binaries.
echo "Configuring system scripts"
echo "Configuring system scripts" >> ${WWIVBASE}/$LOGFILE 2>&1

tar zxvf unix-scripts.tgz >> ${WWIVBASE}/$LOGFILE 2>&1

echo
echo "Setting scripts to use your install location ${WWIVBASE}"
echo "Setting scripts to use your install location ${WWIVBASE}" >> ${WWIVBASE}/$LOGFILE 2>&1
for i in bin/callout.py in.nodemgr wwiv-service .procmailrc .wwivrc
do
    sed -i "s@REPLACE-WWIVBASE@${WWIVBASE}@" $i
done

echo
echo "Your BBS basic data setup is complete."
echo "Please compile and copy in the binary files, then"
echo "run ./init to finalize the BBS.  Then run ./bbs" 
echo "and set up a new user to be the sysop (#1) account"
echo

