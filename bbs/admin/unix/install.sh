#!/bin/bash
#
# install.sh
#
# Runs through the basic steps to install.  We do make certain assumptions
# in order to facilitate the setup.  In particular, we assume we are running
# from the install wwivbase location.

WWIV_DIR=$(pwd)
WWIV_USER=$(whoami)
LOGFILE=install_`date "+%Y-%m-%d_%H-%M-%S"`.log

say() {
  echo $1
  echo $1 > ${WWIV_DIR}/${LOGFILE}
}

confirm () {
    # call with a prompt string or use a default
    read -r -p "${1:-Are you sure? [y/N]} " response
    case $response in
        [yY][eE][sS]|[yY]) 
            true
            ;;
        *)
            false
            ;;
    esac
}

cat <<EOF

 _       ___       __ __ _    __   ______  ___   
| |     / / |     / /  _/ |  / /  / ____/ |__ \ 
| | /| / /| | /| / // / | | / /  /___ \   __/ / 
| |/ |/ / | |/ |/ // /  | |/ /  ____/ /_ / __/  
|__/|__/  |__/|__/___/  |___/  /_____/(_)____/  
                                               
    ____           __        ____      __  _           
   /  _/___  _____/ /_____ _/ / /___ _/ /_(_)___  ____ 
   / // __ \/ ___/ __/ __  / / / __  / __/ / __ \/ __ \\ 
 _/ // / / (__  ) /_/ /_/ / / / /_/ / /_/ / /_/ / / / / 
/___/_/ /_/____/\__/\__,_/_/_/\__,_/\__/_/\____/_/ /_/  
 


EOF

say "Starting the install process."
say ""

say "You are running as: ${WWIV_USER}"
say "Please make sure you are running as the user you will use for WWIV."
echo 
echo 
confirm "Are you running as the correct WWIV user (${WWIV_USER})? " || exit 1

# Make sure init exists
if [ ! -f "${WWIV_DIR}/init" ]
then
    say "init is missing. Please compile your BBS files and copy them"
    say "to ${WWIV_DIR} and run install.sh again."
    exit 1
fi

# configure scripts and helper binaries.
echo 
echo 
say ""
say "Configuring scripts to use the following settings:"

say "WWIV_DIR:  ${WWIV_DIR}"
say "WWIV_USER: ${WWIV_USER}"
echo
echo

for i in systemd/*.template
do
	out=$(echo $i | sed -e 's/\.[^.]*$//')
	echo "Processing: $i > ${out}"
    WWIV_USER=${WWIV_USER} WWIV_DIR=${WWIV_DIR} envsubst < $i > ${out}
done

echo
echo
echo "Your BBS basic systemd service configuration "
echo "and helper scripts are complete."
echo
echo

confirm "Press enter to run ./init to finalize setting up the BBS: "

./init

echo
echo "init complete.  Please run ./bbs and hit the spacebar to login"
echo "and set up a new user to be the sysop (#1) account"
echo
echo "To create a systemd service, please copy the following files (as root):"
echo
echo "sudo cp $(pwd)/systemd/config /etc/wwiv/config"
echo "sudo cp $(pwd)/systemd/wwivd.service /etc/systemd/system/wwivd.service"
