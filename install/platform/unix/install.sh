#!/bin/bash
#
# install.sh
#
# Runs through the basic steps to install.  We do make certain assumptions
# in order to facilitate the setup.  In particular, we assume we are running
# from the install wwivbase location and we also assume we are running as 
# the root user.

RUN=""
declare -r RUNDIR=$(pwd)
wwiv_dir="${RUNDIR}"
wwiv_user="wwiv"
wwiv_group="wwiv"
FORCE="false"
YES=""

show_help() {
  
  printf "$0 - WWIV Bulletin Board System Installation Script.\r\n\n"

  printf "Usage:\r\n\n"
  echo "-d | --directory [directory]  : Directory to install WWIV "
  echo "                                (default ${RUNDIR})"
  echo "-g | --group [group]          : WWIV User's Group (default ${wwiv_group})"
  echo "-f | --force                  : Force - ignore warnings on existing users,"
  echo "                                groups, or software requirements"
  echo "-n | --dry_run                : Dry run mode, do not execute commands"
  echo "-u | --user [username]        : WWIV User (default ${wwiv_user})"
  echo "-y | --yes                    : Don't prompt for confirmation or pause"
  printf "\r\n\n"
}

while :; do
  # echo "param: $1"
  case $1 in
      -h|-\?|--help)
        show_help    # Display a usage synopsis.
        exit
        ;;
      -d|--dir)     # Set wwiv-user
        if [ "$2" ]; then
          wwiv_dir="$2"
          shift
        fi
        ;;
      -f|--force)  # Force
        FORCE="true"
        echo "** FORCE"
        ;;
      -g|--group)     # Set wwiv-user
        if [ "$2" ]; then
          wwiv_group="$2"
          shift
        fi
        ;;
      -n|--dry_run)  # Set dry-run mode
        RUN="echo DRY-RUN: "
        echo "** Dry Run Mode"
        ;;
      -u|--user)     # Set wwiv-user
        if [ "$2" ]; then
          wwiv_user="$2"
          shift
        fi
        ;;
      -y|--yes)  # Force
        YES="true"
        echo "** YES"
        ;;
      --) # Done
        shift
        break
        ;;
      -?*)
        echo "WARN: Unknown option (ignored): $1"
        ;;
      *)
        break;
  esac
  shift
done

# Ensure we're root.
ID=$(id -u)
if [[ $ID -ne 0 ]]; then
  echo "Looks like you are not root.  Please re-run as root."
  echo "${RED}There are tasks that require root to complete."
  exit 20
fi

declare -r WWIV_DIR=${wwiv_dir:-"${RUNDIR}"}
declare -r WWIV_BIN_DIR=${WWIV_BIN_DIR:-"${WWIV_DIR}"}
declare -r WWIV_LOG_DIR=${WWIV_LOG_DIR:-"${WWIV_DIR}/log"}
declare -r WWIV_OS=$(uname -s)
declare -r DATE=`date "+%Y-%m-%d_%H-%M-%S"`
declare -r LOGFILE=${RUNDIR}/install.${DATE}.log
declare -r WWIV_USER=${wwiv_user:-"wwiv"}
declare -r WWIV_GROUP=${wwiv_group:-"wwiv"}

# now that variables are defined, these functions can be used.
source "$(dirname $(realpath $0))/install.bash"

# setting path so running with sudo works
export PATH=/sbin:/usr/sbin:/usr/local/sbin:$PATH

display_banner

if ! confirm "Welcome to the WWIV installer. Continue?"
then
  say "Sorry you don't want to continue.  Hope you come back soon."
  exit 3
fi

# Show menu.

echo
say "Starting the install process."
say ""
say "The install details will be captured in: ${LOGFILE}"


### parse our CLI options ###
# Future getopts for setting values will go here.
# For right now, we just edit the variables at the top of the 
# script for our expected WWIV user, etc.


### check for pre-reqs ###
say "Checking for needed OS tools"

# Critical
MISSING_CRIT=false
checkcrit sudo
checkcrit zip
checkcrit unzip
checkcrit awk

if [[ ${MISSING_CRIT} == "true" ]]; then
  say "You are missing critical tools (listed as ERROR above)"
  if [[ ${FORCE} == "false" ]]; then
    say "Please install the missing tools and start the install again"
    exit 5
  fi
else
  echo
  say "All critical checks passed, continuing with the install."
fi

# Optional
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
fi

say "Checking if wwiv group (${WWIV_GROUP}) exists"
wwiv_group_exists "${WWIV_GROUP}"
group_exist=$?
if [[ "${group_exist}" -eq 0 && "${FORCE}" == "false" ]]; then
  say "User (${WWIV_GROUP}) already exists, too.  Please verify your user"
  say "name choice and re-run the installer or add --force to the commandline."
  exit 3
fi
say "Checking if wwiv user (${WWIV_USER}) exists"
wwiv_user_exists "${WWIV_USER}"
user_exist=$?
if [[ ${user_exist} -eq 0 && "${FORCE}" == "false" ]]; then
  say "User (${WWIV_USER}) already exists, too.  Please verify your user"
  say "name choice and re-run the installer or add --force to the commandline."
  exit 3
fi

check_binaries bbs wwivconfig wwivd wwivutil
say ""
say "Basic Checks completed"

# Making sure the values are what we want
say "Creating the WWIV user, group and directory with the following values:"
say ""
say "WWIV_DIR:     ${YELLOW}${WWIV_DIR}"
say "WWIV_BIN_DIR: ${YELLOW}${WWIV_BIN_DIR}"
say "WWIV_LOG_DIR: ${YELLOW}${WWIV_LOG_DIR}"
say "WWIV_USER:    ${YELLOW}${WWIV_USER}"
say "WWIV_GROUP:   ${YELLOW}${WWIV_GROUP}"
say ""

if ! confirm "Are these correct?"
then
  say ""
  say "Please set the values to what you want in the top of install.sh"
  say "and re-run the script. Exiting."
  exit 1
fi

if ! wwiv_group_exists "${WWIV_USER}"; then
  create_wwiv_group "${WWIV_GROUP}"
fi

if ! wwiv_user_exists "${WWIV_USER}"; then
  say "Creating locked WWIV user account: ${WWIV_USER}"
  create_wwiv_user "${WWIV_USER}" "${WWIV_GROUP}" "${WWIV_DIR}"
  lock_wwiv_user "${WWIV_USER}"
fi

create_directory ${WWIV_DIR}

### configure scripts and helper binaries. ###
say ""
say "Configuring scripts to use the following settings:"

say "WWIV_DIR:  ${WWIV_DIR}"
say "WWIV_USER: ${WWIV_USER}"
say "WWIV_GROUP: ${WWIV_GROUP}"
say ""

# Unzip the unix archive.
unzip -n -q unix.zip

WWIV_TEMPLATE_PATH="systemd/*.template"
if [[ ${WWIV_OS} == "SunOS" ]]; then
  WWIV_TEMPLATE_PATH="svcadm/*.template"
fi
process_templates "${WWIV_TEMPLATE_PATH}"

if [[ ${WWIV_OS} == "Linux" ]]; then
  create_etc_wwiv "${RUNDIR}"
  configure_systemd "${RUNDIR}"
elif [[ ${WWIV_OS} == "SunOS" ]]; then
  configure_svcadm "${RUNDIR}"
fi


#Final permissions setting of the WWIV directory files before running wwivconfig
say "Making sure ${WWIV_USER} owns all the files"
${RUN} chown -R ${WWIV_USER}:${WWIV_GROUP} ${WWIV_DIR} &>> ${LOGFILE}
say ""
say "Your BBS basic systemd service configuration "
say "and helper scripts are complete."

### Final wwivconfig steps ###
say "About to initialize the WWIV BBS data files"
pausescr

say "Initializing data files"
${RUN} sudo -u${WWIV_USER} "${WWIV_DIR}/wwivconfig" --initialize
say "Installation complete"

echo
say "Please log in as your new WWIV user (eg, sudo -u ${WWIV_USER} -s)"
say "and run the ./wwivconfig command to create the SysOp account and"
say "customize the BBS information."
say ""
say "If you need any assistance, check out the docs or find us on IRC."
