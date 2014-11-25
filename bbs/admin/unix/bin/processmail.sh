#!/bin/bash
#
# processmail
#
# script to manage all the steps for processing WWIVnet packets
#

source ~/.wwivrc

date

cd ${WWIVBASE}
${WWIVBASE}/bin/inbound.sh
${WWIVBASE}/bin/outbound.sh

