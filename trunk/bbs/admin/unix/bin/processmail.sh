#!/bin/bash
#
# processmail
#
# script to manage all the steps for processing WWIVnet packets
#

source REPLACE-WWIVBASE/.wwivrc

date

cd ${WWIV_DIR}
${WWIV_DIR}/bin/inbound.sh
${WWIV_DIR}/bin/outbound.sh

