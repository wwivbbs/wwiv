#!/bin/bash
#
# process-inbound
#
# process all the messages that were picked up by fetchmail
# 
# The basic process is: cd to the WWIVNET directory and uudecode
# each file so the corresponding SNN.NET file gets dropped in
# the right place, rather than try to move things around.
#
# I have two different error conditions if SNN.NET exists.
# exit 1 is if SNN.NET exists when we try to start
# exit 2 is if SNN.NET exists during processing.  The idea is
# there's likely a problem (eg, dosemu barfed) so we should
# stop and review/report

source ~/.wwivrc

cd ${WWIVNET}

# get new mail (if any)
fetchmail

# Check for processing in progress and abort if anything is already there
if [ -f s90.net -o -f S90.NET ]
then
    echo "ERROR: an S90.net file already exists"
    exit 1
else
    # Loop over the contents of the inbound directory 
    for msg in `ls ${INBOUND}`
    do
        uudecode ${INBOUND}/${msg}
        sed -i 's/\//\\/g' ${WWIVDATA}/networks.dat
        dosemu -quiet -dumb -E inbound.bat 2>/dev/null | sed -n -e '/^net37/,$'p -e '/PPP Project/,$'p
        sed -i 's/\\/\//g' ${WWIVDATA}/networks.dat

        # Check to see if the file got processed successfully
        if [ -f s90.net -o -f S90.NET ]
        then
            echo "ERROR: ${msg} failed to process successfully. Aborting further processing."
            exit 2
        else
            # File processed succesfully, move to processed as a backup
            mv ${INBOUND}/${msg} ${PROCESSED}
        fi
    done
fi

