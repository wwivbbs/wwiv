#!/bin/sh
# Find paths and names for various Xcode development tools.  This is
# sufficiently complex, and changes frequently enough, that Apple provides
# a tool "xcrun" to query the system for various Xcode-related names and
# paths.  We either pass the output of this back to the caller or extract
# what's being requested from something that xcrun gives us.
#
# Usage: xcode.sh servicename [sdkname]

RESPONSE=""
SERVICENAME=""
SDKNAME=""

# Make sure that we've been given either a single argument consisting of the
# requested service name or the service name and an SDK name.

if [ -z "$1" ] ; then
	echo "Usage: $0 servicename [sdkname]" >&2 ;
	exit 1 ;
fi
SERVICENAME=$1
if [ $# -eq 2 ] ; then
	SDKNAME="--sdk $2" ;
fi

# Find the path to the SDK tools.  xcrun looks for an executable, not a
# directory name, so we look for cc and then strip back the last component
# to get the path to cc, which is also the path to the other tools.

IOS_TOOLS_PATH="$(xcrun $SDKNAME -find cc | sed 's%/[^/]*$%/%')"

# Report the requested Xcode service/information type.

case $SERVICENAME in
	'ar')
		RESPONSE="$(xcrun $SDKNAME -find ar)" ;;

	'bitcode-arg')
		# Return the command-line argument used for embedding bitcode,
		# (LLVM IR that's JIT-compiled to the appropriate iOS platform when
		# run).  This option isn't always available so we extract it from
		# the LLVM help page by searching for the specific string
		# "embed-bitcode " (note the space, since embed-bitcode-xxxx's also
		# exist) and returning it if the compiler documents its support.
		RESPONSE="$($IOS_TOOLS_PATH/cc --help | grep "embed-bitcode " | sed 's/^[[:space:]]*//' | sed 's/ .*//')" ;;

	'cc')
		RESPONSE="$(xcrun $SDKNAME -find cc)" ;;

	'ld')
		RESPONSE="$(xcrun $SDKNAME -find ld)" ;;

	'osversion')
		RESPONSE="$(xcrun $SDKNAME --show-sdk-version | sed 's/[.].*//')" ;;

	'strip')
		RESPONSE="$(xcrun $SDKNAME -find strip)" ;;

	'sysroot')
		RESPONSE="$(xcrun $SDKNAME --show-sdk-path)" ;;

	'sdkversion')
		RESPONSE="$(xcrun $SDKNAME --show-sdk-version)" ;;

	*)
		echo "Unknown service $SERVICENAME" ;
		exit 1 ;;
esac

# Finally, report what we've found

echo "$RESPONSE"
