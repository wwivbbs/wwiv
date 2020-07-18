# install.sh helper library

declare -r PURPLE='\033[1;35m'
declare -r NC='\033[0m'
declare -r GREEN='\033[1;32m'
declare -r YELLOW='\033[1;33m'
declare -r RED='\033[1;31m'
declare -r CYAN='\033[0;36m'
declare -r BLUE='\033[1;34m'

  
say() {
  echo -e "${BLUE}* ${CYAN}$1${NC}"
  echo `date "+%Y-%m-%d_%H-%M-%S"`: $1 >> ${LOGFILE}
}

confirm () {
	if [[ -n "${YES}" ]]; then
		return
	fi
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
	if [[ -z "${YES}" ]]; then
		echo "YES: ${YES}"
	  echo -e "${PURPLE}[PAUSE]${NC}"
	  read -n1 -s
	fi
}

filearch() {
  file -be elf $1 | awk -F, '{print $2}' | sed 's/[- _]//g'
}

# Checks if a binary exists in the path
# Params: [binary]
check_binary_exists() {
	local binary="$1"
	command -v $1 >/dev/null 2>&1
}

checkcrit() {
  if ! check_binary_exists "$1"; then
    printerr "$1 is not installed"
    MISSING_CRIT=true
  fi
}

checkopt() {
  if ! check_binary_exists $1; then
    printwarn "$1 is not installed"
    MISSING_OPT=true
  fi
}

# Checks if the binary is likely the right architecture for
# this system.  
# Note: The binary must exist or it's a fatal error
# Params: [full path to binary]
check_binary_arch_type() {
	local binary="$1"
	if [[ ! -f "${binary}" ]]; then
		printerr "FATAL: check_binary_arch_type: Did not find ${binary}"
		say "${binary} is missing. Please verify your BBS tarball is complete"
		say "and run install.sh again."
		say ""
		say "If you are running on a system that requires compiling the binaries"
		say "manually, please do so and copy them all into ${WWIV_DIR}"
		say "and run install.sh again."
		exit 2;
	fi

	declare -r SYSTYPE=$(uname -m|sed 's/[- _]//g')
	declare -r INIT_TYPE=$(filearch ${binary})
	[[ $SYSTYPE =~ $INIT_TYPE ]]
}

check_binary() {
	local binary=${WWIV_BIN_DIR}/$1
	if ! check_binary_arch_type "${binary}"; then
		printwarn "Found ${binary}, continuing.  But..."
		say "Your system type does not match the filetype of ${binary}."
		say "System type is ${SYSTYPE} and file reports ${INIT_TYPE}."
		say "If things are too different, it might not work"
		say "Please refer to the documentation about compiling yourself"
		say "if things don't look right or ${binary} doesn't run."
		echo
		if ! confirm "Should we continue the install?"; then
			echo
			say "Aborting the install.  Please verify your binaries and re-run the installer."
			exit 2
	  fi
	fi
}

check_binaries() {
  while [[ -n $1 ]]; do
  	echo "Checking $1"
    check_binary $1
    shift
  done
}

create_directory() {
  local dir=$1
  MKDIR="${RUN} mkdir"
  # Validate WWIV directory
  if [[ ! -d ${dir} ]]; then
  	say "The directory ${dir} doesn't exist; creating it."
  	if ! ${MKDIR} -p ${dir}  |& tee -a ${LOGFILE}
  	then
	    say "There was an error creating the directory"
	    say "Please check $LOGFILE for details.  Aborting."
	    exit 10
  	else
  	    say "Directory ${dir} created"
  	fi
  fi
}

# Does a user exist. 
# Params: [username]
wwiv_user_exists() {
	local user="$1"
	id "${WWIV_USER}" > /dev/null 2>&1
}

# Creates a locked WWIV service user.
# Params: [username] [group] [home]
create_wwiv_user() {
	echo "create_wwiv_user"
	local wwiv_user="$1"
	local wwiv_group="$2"
	local wwiv_home="$3"
	${RUN} useradd  -g ${wwiv_group} \
									-c "WWIV BBS Service Account" \
									-d ${wwiv_home} \
					  			-s /sbin/nologin \
					  			-m ${wwiv_user} |& tee -a ${LOGFILE}
}

# Locks an existing WWIV service user.
# Params: [username]
lock_wwiv_user() {
	local wwiv_user="$1"
	# Lock the wwiv user.
	${RUN} usermod -L ${wwiv_user} |& tee -a ${LOGFILE}
}

# Does the WWIV group exist.
# params: [groupname]
wwiv_group_exists() {
	local wwiv_group="$1"
	grep -q "^${WWIV_GROUP}:" /etc/group
}

# Creates a group for the locked WWIV service user.
# Params: [groupname]
create_wwiv_group() {
	local wwiv_group="$1"
	${RUN} groupadd "${wwiv_group}" |& tee -a ${LOGFILE}
}

# Checks to see if an existing WWIV installation exists at dir.
# Params: [dir]
check_existing_wwiv_install() {
	local dir="$1"
	[[ -e "${dir}/config.dat" ]]
}

# Process all template files with env subst
# Params: [globspec]
process_templates() {
	local globspec="$1"
	# Don't quote globspec or it will not expand
  for i in ${globspec}
  do 
    out=${i%%.template}
    say "Processing: $i > ${out}"
    ${RUN} sed -e "s|\${WWIV_DIR}|${WWIV_DIR}|g" -e "s|\${WWIV_USER}|${WWIV_USER}|g" $i > ${out}
  done
}

# Creates /etc/wwiv and copies the processed template
# files to it.
# Params: [dir: Where the installer is run from]
create_etc_wwiv() {
	local dir="$1"
  say "Copying the wwiv config file to /etc/wwiv"
  if [[ ! -d /etc/wwiv ]]; then
    ${RUN} mkdir -p /etc/wwiv  |& tee -a ${LOGFILE}
  elif [[ -e /etc/wwiv/config ]]; then
    say "/etc/wwiv/config already exists, saving as /etc/wwiv/config.new"
    ${RUN} cp ${dir}/systemd/config /etc/wwiv/config.new  |& tee -a ${LOGFILE}
  fi
  ${RUN} cp ${dir}/systemd/config /etc/wwiv/config  |& tee -a ${LOGFILE}
  say "Installed: /etc/wwiv/config"
}

# Configures systemd by copying the templated install
# files to it.
# Params: [dir: Where the installer is run from]
configure_systemd() {
	local dir="$1"
	if [[ ! -d /etc/systemd ]]; then
	  say "You don't appear to be using systemd. You may need some more customization"
	  say "to run the wwivd service on a non-systemd setup.  Please check the docs"
	  say "or the IRC channel for additional help."
	  return 255;
	fi

	if [[ -e /etc/systemd/system/wwivd.service ]]; then
    say "/etc/systemd/system/wwivd.service already exists"
    say "Installed /etc/systemd/system/wwivd.service.new"
    ${RUN} cp ${dir}/systemd/wwivd.service /etc/systemd/system/wwivd.service.new  |& tee -a ${LOGFILE}
  else
    ${RUN} cp ${dir}/systemd/wwivd.service /etc/systemd/system/wwivd.service |& tee -a ${LOGFILE}
    say "Installed: /etc/systemd/system/wwivd.service"
	  ${RUN} systemctl daemon-reload |& tee -a ${LOGFILE}
  fi
  ${RUN} systemctl enable wwivd.service |& tee -a ${LOGFILE}
  say "systemd wwivd service enabled"
}

# Configures Solaris svcadm by copying the templated install
# files to it.
# Params: [RUNDIR: Where the installer is run from]
configure_svcadm() {
	local RUNDIR="$1"
  if [[ -e /var/svc/manifest/application/wwivd.xml ]]; then
  	say "/var/svc/manifest/application/wwivd.xml already exists, aborting svcadm install"
  	return 255
	fi
  if [[ ! -e ${WWIV_DIR}/start_wwiv.sh ]]; then
    say "Installing ${RUNDIR}/svcadm/start_wwiv.sh"
    ${RUN} cp ${RUNDIR}/svcadm/start_wwiv.sh ${WWIV_DIR} |& tee -a ${LOGFILE}
    ${RUN} chmod +x ${WWIV_DIR}/start_wwiv.sh |& tee -a ${LOGFILE}
  fi
  say "Installing service manifest."
  ${RUN} cp ${RUNDIR}/svcadm/wwivd.xml /var/svc/manifest/application/wwivd.xml |& tee -a ${LOGFILE}
  ${RUN} svcadm restart svc:/system/manifest-import |& tee -a ${LOGFILE}
  say "service manifest installed, to enable run : \"svcadm enable wwivd\""
}


display_banner() {
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
}

