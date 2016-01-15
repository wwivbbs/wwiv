#!/bin/bash
#
# processmail
#
# script to manage all the steps for processing WWIVnet packets
#
# This only covers 

source REPLACE-WWIVBASE/.wwivrc

date

cd ${WWIV_DIR}

# I'd like to eventually be able to loop over the list of 
# nets from init data

# Calls out to @1 of WWIVnet
./networkb --send --network=wwivnet --node=1 --bbsdir=${WWIV_DIR}

