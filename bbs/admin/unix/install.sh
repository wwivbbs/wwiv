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

#
# Make sure we have the necessary tools
#
# checking for unzip shouldn't be necessary (since we needed unzip 
# to get this file in the first place)
#
which unzip >> ${WWIV_DIR}/$LOGFILE 2>&1
STATUS=$?

if [ "$STATUS" -ne "0" ]
then 
    echo "unzip utility is missing.  Please install it and run install again" 
    echo "unzip utility is missing.  Please install it and run install again" >> ${WWIV_DIR}/$LOGFILE 2>&1
    exit 1
fi

# Make sure init exists
if [ ! -f ${WWIV_DIR}/init ]
then
    echo "init is missing.  Please compile your BBS files and copy them"
    echo "init is missing.  Please compile your BBS files and copy them" >> ${WWIV_DIR}/$LOGFILE 2>&1
    echo "to ${WWIV_DIR} and run sh install.sh again."
    echo "to ${WWIV_DIR} and run sh install.sh again." >> ${WWIV_DIR}/$LOGFILE 2>&1
    exit 1
fi

#
# backup Windows binaries since we don't use them and they cause conflicts
#
echo 
echo "Moving Windows binaries"
echo "Moving Windows binaries" >> ${WWIV_DIR}/$LOGFILE 2>&1
mkdir win-bins
mv *.exe win-bins


#
# Unzip the data files
#
echo
echo "Configuring data directories"
echo "Configuring data directories" >> ${WWIV_DIR}/$LOGFILE 2>&1

# make directories
if [ ! -d gfiles ]
then 
    mkdir gfiles >> ${WWIV_DIR}/$LOGFILE 2>&1
fi

if [ ! -d data ]
then 
    mkdir data >> ${WWIV_DIR}/$LOGFILE 2>&1
fi

# unzip menus
cd gfiles
unzip -u ../en-menus.zip >> ${WWIV_DIR}/$LOGFILE 2>&1
cd ${WWIV_DIR}

# unzip regions
cd data
unzip -u ../regions.zip >> ${WWIV_DIR}/$LOGFILE 2>&1
cd ${WWIV_DIR}

# unzip zip-city
cd data
unzip -u ../zip-city.zip >> ${WWIV_DIR}/$LOGFILE 2>&1
cd ${WWIV_DIR}


# configure scripts and helper binaries.
echo "Configuring system scripts"
echo "Configuring system scripts" >> ${WWIV_DIR}/$LOGFILE 2>&1

tar zxvf unix-scripts.tgz >> ${WWIV_DIR}/$LOGFILE 2>&1

echo
echo "Setting file permissions"
echo "Setting file permissions" >> ${WWIV_DIR}/$LOGFILE 2>&1
chmod 600 .fetchmailrc .procmailrc >> ${WWIV_DIR}/$LOGFILE 2>&1

echo
echo "Setting scripts to use your install location ${WWIV_DIR}"
echo "Setting scripts to use your install location ${WWIV_DIR}" >> ${WWIV_DIR}/$LOGFILE 2>&1
for i in bin/inbound.sh bin/outbound.sh bin/callout.py bin/processmail.sh bin/daily-cleanup.sh .wwivrc in.nodemgr .procmailrc wwiv-service
do
    sed -i "s@REPLACE-WWIVBASE@${WWIV_DIR}@" $i
done

echo
echo "Your BBS basic data setup is complete."
echo "running ./init now to finalize the BBS.  "
sleep 5

cd ${WWIV_DIR}
./init

echo
echo "init complete.  Please run ./bbs to login "
echo "and set up a new user to be the sysop (#1) account"
echo
