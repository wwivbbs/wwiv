#!/bin/bash
#
# install.sh
#
# Runs through the basic steps to install.  We do make certain assumptions
# in order to facilitate the setup.  In particular, we assume we are running
# from the install wwivbase location and we also assume we are running as 
# the root user.

### Default values ###
# We assume our install location is the current directory
# You may change these values to whatever you want, but we
# recommend leaving them as-is
WWIV_DIR=$(pwd)
WWIV_USER=wwiv
WWIV_GROUP=wwiv


### Do not edit below this line ###

RUNDIR=$(pwd)

DATE=`date "+%Y-%m-%d_%H-%M-%S"`

LOGFILE=${RUNDIR}/install_${DATE}.log

# setting path so running with sudo works
export PATH=/sbin:/usr/sbin:/usr/local/sbin:$PATH

say() {
  echo $1
  echo `date "+%Y-%m-%d_%H-%M-%S"`: $1 >> ${LOGFILE}
}

confirm () {
  read -r -p "$1 [y/N]: " response
  case $response in
    [yY]) 
        true
        ;;
    *)
        false
        ;;
  esac
}

printok() {
  GREEN='\033[0;32m'
  NC='\033[0m'
  echo -e "${GREEN}OK: $1${NC}"
  echo `date "+%Y-%m-%d_%H-%M-%S"`: OK: $1 >> ${LOGFILE}
}

printwarn() {
  YELLOW='\033[0;33m'
  NC='\033[0m'
  echo -e "${YELLOW}WARNING: $1${NC}"
  echo `date "+%Y-%m-%d_%H-%M-%S"`: WARNING: $1 >> ${LOGFILE}
}

printerr() {
  RED='\033[0;31m'
  NC='\033[0m'
  echo -e "${RED}ERROR: $1${NC}"
  echo `date "+%Y-%m-%d_%H-%M-%S"`: ERROR: $1 >> ${LOGFILE}
}

### Install header ###
cat <<HEADER


 _       ___       __ __ _    __   ______  _____
| |     / / |     / /  _/ |  / /  / ____/ |__  /
| | /| / /| | /| / // / | | / /  /___ \    /_ <
| |/ |/ / | |/ |/ // /  | |/ /  ____/ /_ ___/ /
|__/|__/  |__/|__/___/  |___/  /_____/(_)____/

    ____           __        ____      __  _           
   /  _/___  _____/ /_____ _/ / /___ _/ /_(_)___  ____ 
   / // __ \/ ___/ __/ __  / / / __  / __/ / __ \/ __ \\ 
 _/ // / / (__  ) /_/ /_/ / / / /_/ / /_/ / /_/ / / / / 
/___/_/ /_/____/\__/\__,_/_/_/\__,_/\__/_/\____/_/ /_/  
 


HEADER

if ! confirm "Welcome to the WWIV installer. Continue?"
then
  say "Sorry you don't want to continue.  Hope you come back soon."
  echo
  exit 3
fi

say ""
say "Starting the install process."
say ""



### parse our CLI options ###
# Future getopts for setting values will go here.
# For right now, we just edit the variables at the top of the 
# script for our expected WWIV user, etc.


say "Doing some basic sanity checks"
echo

if [[ -e ${WWIV_DIR}/config.dat ]]
then
  say "Looks like there's already a WWIV install in ${WWIV_DIR}."
  say "If you really meant to install, please check the target directory"
  say "before trying again.  Aborting the install."
  say ""
  exit 2
else
  say "No config.dat found, continuing."
fi

if [[ -f "${WWIV_DIR}/init" ]]
then
  say "found init, continuing."
else
  say "init is missing. Please verify your BBS tarball is complete"
  say "and run install.sh again."
  say ""
  say "If you are running on a system that requires compiling the binaries"
  say "manually, please do so and copy them all into ${WWIV_DIR}"
  say "and run install.sh again."
  say ""
  exit 2
fi
say "Basic Checks passed"
echo

say "Checking for needed OS tools"
echo
### check for pre-reqs ###
#Critical
MISSING_CRIT=false

if which sudo > /dev/null 2>&1
then
  printok "sudo is installed"
else
  printerr "sudo is not installed"
  MISSING_CRIT=true
fi

if which zip > /dev/null 2>&1
then
  printok "zip is installed"
else
  printerr "zip is not installed"
  MISSING_CRIT=true
fi

if which unzip > /dev/null 2>&1
then
  printok "unzip is installed"
else
  printerr "unzip is not installed"
  MISSING_CRIT=true
fi

if [[ ${MISSING_CRIT} == "true" ]]
then
  echo
  say "You are missing critical tools (listed as ERROR above)"
  say "Please install the missing tools and start the install again"
  exit 5
else
  echo
  say "All critical checks passed, continuing with the install."
fi

#Optional
which dos2unix > /dev/null 2>&1; D2U=$?
which unix2dos > /dev/null 2>&1; U2D=$?
which dosemu > /dev/null 2>&1; DOSEMU=$?
which screen > /dev/null 2>&1; SCREEN=$?

# ncurses


# Making sure the values are what we want
cat <<VARCHECK

Creating the WWIV user, group and directory with the following values:

WWIV_DIR = ${WWIV_DIR}
WWIV_USER = ${WWIV_USER}
WWIV_GROUP = ${WWIV_GROUP}

VARCHECK

if ! confirm "Are these correct?"
then
  echo
  say "Please set the values to what you want in the top of install.sh"
  say "and re-run the script. Exiting."
  echo
  exit 1
fi


# Validate WWIV directory
if [[ ! -d ${WWIV_DIR} ]]
then
  say "The directory ${WWIV_DIR} doesn't exist; creating it."
  if ! mkdir -p ${WWIV_DIR} >> $LOGFILE 2>&1
  then
    say "There was an error creating the directory"
    say "Please check $LOGFILE for details.  Aborting."
    exit 10
  else
    say "Directory ${WWIV_DIR} created"
  fi
fi

### Create WWIV user and group ###
# does wwiv group exist? 
if grep -q "^${WWIV_GROUP}:" /etc/group
then
  if confirm "Group (${WWIV_GROUP}) already exists. Should we use it (DANGEROUS)?"
  then
    say "Proceeding with existing group (${WWIV_GROUP})"
  else
    read -r -p "Please enter a new group name: " WWIV_GROUP
    if grep -q "^${WWIV_GROUP}:" /etc/group
    then
      say "Group (${WWIV_GROUP}) already exists, too.  Please verify your group"
      say "name choice and re-run the installer.  Aborting."
      exit 3
    else
      say "Using new group ${WWIV_GROUP}"
      groupadd ${WWIV_GROUP}
      say "Group ${WWIV_GROUP} created."
    fi
  fi
else
  say "Group ${WWIV_GROUP} not found; creating it."
  groupadd ${WWIV_GROUP}
  say "Group ${WWIV_GROUP} created."
fi

# does wwiv user exist?
if id "${WWIV_USER}" > /dev/null 2>&1
then
  if confirm "User (${WWIV_USER}) already exists. Should we use it (DANGEROUS)?"
  then
    say "Proceeding with existing user (${WWIV_GROUP})"
  else
    read -r -p "Please enter a new user name: " WWIV_USER
    if id "${WWIV_USER}" > /dev/null 2>&1
    then
      say "user (${WWIV_USER}) already exists, too.  Please verify your user"
      say "name choice and re-run the installer.  Aborting."
      exit 4
    else
      say "Using new user ${WWIV_USER}."
      useradd -g ${WWIV_GROUP} -c "WWIV BBS Service Account" -d ${WWIV_DIR} \
	      -s /sbin/nologin -m ${WWIV_USER} >> ${LOGFILE} 2>&1
      say "User ${WWIV_USER} created."
    fi
  fi
else
  say "User ${WWIV_USER} not found; creating it."
  useradd -g ${WWIV_GROUP} -c "WWIV BBS Service Account" -d ${WWIV_DIR} \
	  -s /sbin/nologin -m ${WWIV_USER} >> ${LOGFILE} 2>&1
  say "User ${WWIV_USER} created."
fi

say "locking ${WWIV_USER} user account (we don't want direct access to it)."
usermod -L ${WWIV_USER}


# Change ownership of WWIV_DIR so our WWIV user can write to it
chown -R ${WWIV_USER}:${WWIV_GROUP} ${WWIV_DIR} >> ${LOGFILE} 2>&1


# prompt for userid that should be set up in sudo to have access to the WWIV user
read -r -p "Please enter the userid that will have sudo access to ${WWIV_USER}: " SUDOUSER
say "Adding ${SUDOUSER} to the sudoers file"
echo "${SUDOUSER}   ALL=(${WWIV_USER}) ALL" >> /etc/sudoers
say "The user ${SUDOUSER} can now run all commands as ${WWIV_USER} via sudo"


### configure scripts and helper binaries. ###
echo 
say ""
say "Configuring scripts to use the following settings:"

say "WWIV_DIR:  ${WWIV_DIR}"
say "WWIV_USER: ${WWIV_USER}"
echo
echo

for i in systemd/*.template
do
  out=${i%%.template}
  say "Processing: $i > ${out}"
  sed -e "s|\${WWIV_DIR}|${WWIV_DIR}|g" -e "s|\${WWIV_USER}|${WWIV_USER}|g" $i > ${out}
done

say "Copying the wwiv config file to /etc/wwiv"
if [[ ! -d /etc/wwiv ]]
then
  mkdir /etc/wwiv
  cp ${RUNDIR}/systemd/config /etc/wwiv/config
  say "/etc/wwiv/config file copied"
else
  if [[ -e /etc/wwiv/config ]]
  then
    say "/etc/wwiv/config already exists, saving as /etc/wwiv/config.new"
    cp ${RUNDIR}/systemd/config /etc/wwiv/config.new
    say "/etc/wwiv/config.new file copied"
  else
    cp ${RUNDIR}/systemd/config /etc/wwiv/config
    say "/etc/wwiv/config file copied"
  fi
fi

#are we using systemd?
if [[ ! -d /etc/systemd ]]
then
  say "You don't appear to be using systemd. You may need some more customization"
  say "to run the wwivd service on a non-systemd setup.  Please check the docs"
  say "or the IRC channel for additional help."
else
  if [[ -e /etc/systemd/system/wwivd.service ]]
  then
    say "/etc/systemd/system/wwivd.service already exists, saving as wwivd.service.new"
    cp ${RUNDIR}/systemd/wwivd.service /etc/systemd/system/wwivd.service.new
    say "/etc/systemd/system/wwivd.service.new file copied"
  else
    cp ${RUNDIR}/systemd/wwivd.service /etc/systemd/system/wwivd.service
    say "/etc/systemd/system/wwivd.service file copied"
  fi
fi

#Final permissions setting of the WWIV directory files before running init
chown -R ${WWIV_USER}:${WWIV_GROUP} ${WWIV_DIR} >> ${LOGFILE} 2>&1

### Final init steps ###
echo
echo "Your BBS basic systemd service configuration "
echo "and helper scripts are complete."
echo

say "Initializing data files"
su -c "${WWIV_DIR}/init --initialize" -s /bin/bash ${WWIV_USER}
sleep 3
say "Installation complete"

echo
echo "Please log in as your new WWIV user (eg, sudo -u wwiv -s)"
echo "and run the ./init command to create the SysOp account and"
echo "customize the BBS information."
echo
echo "If you need any assistance, check out the docs or find us on IRC."
echo
