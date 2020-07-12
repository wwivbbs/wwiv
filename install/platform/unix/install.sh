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
WWIV_BIN_DIR=$(pwd)
WWIV_LOG_DIR=$(pwd)/log
WWIV_USER=wwiv
WWIV_GROUP=wwiv
declare -r WWIV_OS=$(uname -s)

### Do not edit below this line ###

RUNDIR=$(pwd)

DATE=`date "+%Y-%m-%d_%H-%M-%S"`

LOGFILE=${RUNDIR}/install.${DATE}.log

# setting path so running with sudo works
export PATH=/sbin:/usr/sbin:/usr/local/sbin:$PATH

declare -r PURPLE='\033[1;35m'
declare -r NC='\033[0m'
declare -r GREEN='\033[1;32m'
declare -r YELLOW='\033[1;33m'
declare -r RED='\033[1;31m'
declare -r CYAN='\033[0;36m'
declare -r BLUE='\033[1;34m'
  
say() {
  echo -e "${CYAN}$1${NC}"
  echo `date "+%Y-%m-%d_%H-%M-%S"`: $1 >> ${LOGFILE}
}

sayl() {
  echo -e "${CYAN}$1${NC}"
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
  echo -e "${GREEN}OK: $1${NC}"
  echo `date "+%Y-%m-%d_%H-%M-%S"`: OK: $1 >> ${LOGFILE}
}

printwarn() {
  echo -e "${YELLOW}WARNING: $1${NC}"
  echo `date "+%Y-%m-%d_%H-%M-%S"`: WARNING: $1 >> ${LOGFILE}
}

printerr() {
  echo -e "${RED}ERROR: $1${NC}"
  echo `date "+%Y-%m-%d_%H-%M-%S"`: ERROR: $1 >> ${LOGFILE}
}

pausescr() {
  echo -e "${PURPLE}[PAUSE]${NC}"
  read -n1 -s
}

filearch() {
  file -be elf $1 | awk -F, '{print $2}' | sed 's/[- _]//g'
}

checkcrit() {
  if which $1 > /dev/null 2>&1
  then
    printok "$1 is installed"
  else
    printerr "$1 is not installed"
    MISSING_CRIT=true
  fi
}

checkopt() {
  if which $1 > /dev/null 2>&1
  then
    printok "$1 is installed"
  else
    printwarn "$1 is not installed"
    MISSING_OPT=true
  fi
}

check_binary() {
    local binary=${WWIV_BIN_DIR}/$1
    if [[ -f "${binary}" ]]; then
	typeset -l SYSTYPE
	typeset -l INIT_TYPE
	SYSTYPE=$(uname -m|sed 's/[- _]//g')
	INIT_TYPE=$(filearch ${binary})
	if [[ $SYSTYPE =~ $INIT_TYPE ]]; then
	    printok "Found ${binary} and architectures appear to match, continuing."
	else
	    printwarn "Found ${binary}, continuing.  But..."
	    say "Your system type does not match the filetype of ${binary}."
	    say "System type is ${SYSTYPE} and file reports ${INIT_TYPE}."
	    say "If things are too different, it might not work"
	    say "Please refer to the documentation about compiling yourself"
	    say "if things don't look right or ${binary} doesn't run."
	    echo
	    if ! confirm "Should we continue the install?"
	    then
		echo
		say "Aborting the install.  Please verify your binaries and re-run the installer."
		exit 2
	    fi
	fi
    else
	printerr "Did not find ${WWIV_BIN_DIR}/${binary}; aborting"
	say "${binary} is missing. Please verify your BBS tarball is complete"
	say "and run install.sh again."
	say ""
	say "If you are running on a system that requires compiling the binaries"
	say "manually, please do so and copy them all into ${WWIV_DIR}"
	say "and run install.sh again."
	say ""
	exit 2
    fi
}

check_binaries() {
    while [[ -n $1 ]]; do
	check_binary $1
	shift
    done
}

echo -e "${BLUE}"
### Install header ###
cat <<HEADER


 _       ___       __ __ _    __   ______
| |     / / |     / /  _/ |  / /  / ____/
| | /| / /| | /| / // / | | / /  /___ \\
| |/ |/ / | |/ |/ // /  | |/ /  ____/ /
|__/|__/  |__/|__/___/  |___/  /_____/

    ____           __        ____      __  _           
   /  _/___  _____/ /_____ _/ / /___ _/ /_(_)___  ____ 
   / // __ \/ ___/ __/ __  / / / __  / __/ / __ \/ __ \\
 _/ // / / (__  ) /_/ /_/ / / / /_/ / /_/ / /_/ / / / / 
/___/_/ /_/____/\__/\__,_/_/_/\__,_/\__/_/\____/_/ /_/  
 


HEADER
echo -e "${NC}"

# make sure we are root.
ID=$(id -u)
if [[ $ID -ne 0 ]]; then
  say "Looks like you are not root.  Please re-run as root."
  sayl "${RED}There are tasks that require root to complete."
  exit 20
fi

if ! confirm "Welcome to the WWIV installer. Continue?"
then
  say "Sorry you don't want to continue.  Hope you come back soon."
  echo
  exit 3
fi

echo
say "Starting the install process."
say ""
say "The install details will be captured in:"
say "${LOGFILE}"


### parse our CLI options ###
# Future getopts for setting values will go here.
# For right now, we just edit the variables at the top of the 
# script for our expected WWIV user, etc.


### check for pre-reqs ###
say "Checking for needed OS tools"
echo

#Critical
MISSING_CRIT=false
checkcrit sudo
checkcrit zip
checkcrit unzip
checkcrit awk

if [[ ${MISSING_CRIT} == "true" ]]; then
  echo
  say "You are missing critical tools (listed as ERROR above)"
  say "Please install the missing tools and start the install again"
  exit 5
else
  echo
  say "All critical checks passed, continuing with the install."
fi

#Optional
MISSING_OPT=false
checkopt dos2unix
checkopt unix2dos
checkopt dosemu
checkopt screen

if [[ ${MISSING_OPT} == "true" ]]; then
  echo
  say "You are missing some optional tools (listed as WARNING above)"
  say "These are not necessary for a basic system, but you may want"
  say "to install the missing tools later; continuing."
  echo
else
  echo
  say "All optional tools checks passed, continuing with the install."
  echo
fi
pausescr

# ncurses


echo
say "Doing some basic sanity checks"
echo

if [[ -e "${WWIV_DIR}/config.dat" ]]; then
  printerr "Looks like there's already a WWIV install in ${WWIV_DIR}."
  say "If you really meant to install, please check the target directory"
  say "before trying again.  Aborting the install."
  say ""
  exit 2
else
  printok "No config.dat found, continuing."
fi


check_binaries bbs wwivconfig wwivd wwivutil
echo
say "Basic Checks completed"
echo



# Making sure the values are what we want
sayl "Creating the WWIV user, group and directory with the following values:"
echo
echo
sayl "WWIV_DIR:     ${YELLOW}${WWIV_DIR}"
sayl "WWIV_BIN_DIR: ${YELLOW}${WWIV_BIN_DIR}"
sayl "WWIV_LOG_DIR: ${YELLOW}${WWIV_LOG_DIR}"
sayl "WWIV_USER:    ${YELLOW}${WWIV_USER}"
sayl "WWIV_GROUP:   ${YELLOW}${WWIV_GROUP}"
echo
echo

if ! confirm "Are these correct?"
then
  echo
  say "Please set the values to what you want in the top of install.sh"
  say "and re-run the script. Exiting."
  echo
  exit 1
fi

create_directory() {
    local dir=$1
    # Validate WWIV directory
    if [[ ! -d ${dir} ]]; then
	say "The directory ${dir} doesn't exist; creating it."
	if ! mkdir -p ${dir} >> $LOGFILE 2>&1
	then
	    say "There was an error creating the directory"
	    say "Please check $LOGFILE for details.  Aborting."
	    exit 10
	else
	    say "Directory ${dir} created"
	fi
    fi
}

create_directory ${WWIV_DIR}

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
echo
read -r -p "Please enter the userid that will have sudo access to ${WWIV_USER}: " SUDOUSER
say "Adding ${SUDOUSER} to the sudoers file"
echo "${SUDOUSER}   ALL=(${WWIV_USER}) ALL" >> /etc/sudoers
say "The user ${SUDOUSER} can now run all commands as ${WWIV_USER} via sudo"
pausescr

### configure scripts and helper binaries. ###
echo 
say ""
say "Configuring scripts to use the following settings:"

say "WWIV_DIR:  ${WWIV_DIR}"
say "WWIV_USER: ${WWIV_USER}"
say "WWIV_GROUP: ${WWIV_GROUP}"
echo
echo

if [[ ${WWIV_OS} == "Linux" ]]; then
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

elif [[ ${WWIV_OS} == "SunOS" ]]; then
  for i in svcadm/*.template
  do
    out=${i%%.template}
    say "Processing: $i > ${out}"
    sed -e "s|\${WWIV_DIR}|${WWIV_DIR}|g" -e "s|\${WWIV_USER}|${WWIV_USER}|g"  -e "s|\${WWIV_GROUP}|${WWIV_GROUP}|g" $i > ${out}
  done
fi

#are we using systemd?
if [[ ${WWIV_OS} == "Linux" ]]; then
  if [[ ! -d /etc/systemd ]]
  then
    say "You don't appear to be using systemd. You may need some more customization"
    say "to run the wwivd service on a non-systemd setup.  Please check the docs"
    say "or the IRC channel for additional help."
  else
    if [[ -e /etc/systemd/system/wwivd.service ]]; then
      say "/etc/systemd/system/wwivd.service already exists, saving as wwivd.service.new"
      cp ${RUNDIR}/systemd/wwivd.service /etc/systemd/system/wwivd.service.new
      say "/etc/systemd/system/wwivd.service.new file copied"
    else
      cp ${RUNDIR}/systemd/wwivd.service /etc/systemd/system/wwivd.service
      say "/etc/systemd/system/wwivd.service file copied"
      say "enabling the wwivd service"
      systemctl daemon-reload
      systemctl enable wwivd.service
      say "systemd wwivd service enabled"
    fi
  fi
elif [[ ${WWIV_OS} == "SunOS" ]]; then
  if [[ -e /var/svc/manifest/application/wwivd.xml ]]; then
    say "/var/svc/manifest/application/wwivd.xml already exists, aborting svcadm install"
  else
    if [[ -e ${WWIV_DIR}/start_wwiv.sh ]]; then
      say "${WWIV_DIR}/start_wwiv.sh exists, not overwriting."
    else
      say "Installing ${RUNDIR}/svcadm/start_wwiv.sh"
      cp ${RUNDIR}/svcadm/start_wwiv.sh ${WWIV_DIR}
      chmod +x ${WWIV_DIR}/start_wwiv.sh
    fi
    say "Installing service manifest."
    cp ${RUNDIR}/svcadm/wwivd.xml /var/svc/manifest/application/wwivd.xml
    svcadm restart svc:/system/manifest-import
    say "service manifest installed, to enable run : \"svcadm enable wwivd\""
  fi
fi


#Final permissions setting of the WWIV directory files before running wwivconfig
echo
say "Making sure ${WWIV_USER} owns all the files"
chown -R ${WWIV_USER}:${WWIV_GROUP} ${WWIV_DIR} >> ${LOGFILE} 2>&1
echo
sayl "Your BBS basic systemd service configuration "
sayl "and helper scripts are complete."
echo

### Final wwivconfig steps ###
sayl "About to initialize the WWIV BBS data files"
pausescr

say "Initializing data files"
su -c "${WWIV_DIR}/wwivconfig --initialize" -s /bin/bash ${WWIV_USER}
say "Installation complete"

echo
sayl "Please log in as your new WWIV user (eg, sudo -u ${WWIV_USER} -s)"
sayl "and run the ./wwivconfig command to create the SysOp account and"
sayl "customize the BBS information."
sayl ""
sayl "If you need any assistance, check out the docs or find us on IRC."
