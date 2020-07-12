#!/bin/bash
#
# processmail
#
# script to manage all the steps for processing WWIVnet packets
#
# This only covers a basic connetion to the main WWIVnet node.
# You can add multiple network calls if you add other nets.

source /etc/wwiv/config

date

cd ${WWIV_DIR}

# I'd like to eventually be able to loop over the list of 
# nets from wwivconfig data.  Currently it has to be done manually

# Calls out to @1 of WWIVnet (assuming your first network is 0)
./network /n1

