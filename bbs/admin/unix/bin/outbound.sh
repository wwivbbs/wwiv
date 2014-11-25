#!/bin/bash
#
# outbound.sh
#
# Process all outgoing mail messages
# 

source ~/.wwivrc

cd ${WWIVNET}

# Get array list of SNN files to process
sxx_list=( `find . -maxdepth 1 -type f -iname "s*.net"` )

if [ ${#sxx_list[@]} -gt 0 ]
then
    if [ ${#sxx_list[@]} -eq 1 ]
    then
        echo "Found ${#sxx_list[@]} file to process: ${sxx_list[@]}"
    else
        echo "Found ${#sxx_list[@]} files to process: ${sxx_list[@]}"
    fi
    sed -i 's/\//\\/g' ${WWIVDATA}/networks.dat
    dosemu -quiet -dumb -E network.bat 2>/dev/null | sed -n -e '/^net37/,$'p -e '/PPP Project/,$'p
    sed -i 's/\\/\//g' ${WWIVDATA}/networks.dat

    # Check to see if the files got processed successfully
    sxx_list=( `find . -maxdepth 1 -type f -iname "s*.net"` )
    if [ ${#sxx_list[@]} -gt 0 ]
    then
        echo "ERROR: failed to process all files successfully. Aborting further processing."
        exit 2
    else
        ${WWIVBASE}/bin/callout.py
    fi
else
    echo "No Mail to send"
fi

