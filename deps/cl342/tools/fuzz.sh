#!/bin/bash
# Fuzz cryptlib (via the spcially-built testlib).

INPUT_DIR_PARENT=afl-in
OUTPUT_DIR_PARENT=afl-out
PROGNAME_SRC=./testlib
PROGNAME=./fuzz-clib
FUZZER=../afl-1*/afl-fuzz
FUZZTYPES="base64 certificate certchain certreq cms pgp pkcs12 pkcs15 pgppub pgpsec \
		   ssl-client ssl-server ssh-client ssh-server tsp-client tsp-server \
		   ocsp-client ocsp-server rtcs-client rtcs-server http-req http-resp"
FUZZTYPE=$1
DIRNAME=${FUZZTYPE}
INPUT_DIR=${INPUT_DIR_PARENT}/${DIRNAME}
OUTPUT_DIR=${OUTPUT_DIR_PARENT}/${DIRNAME}
NO_CPUS=`getconf _NPROCESSORS_ONLN`

# Without ASAN the fuzzing uses -m 200 to give 200MB of memory which for
# some reason is needed by AFL, with ASAN it needs to be -m none to set no
# limit.

MEMORY_LIMIT=none

# Make sure that we've been given sufficient arguments.

if [ -z "$1" ] ; then
	echo "$0: Missing option.  Valid options are:" >&2
	echo "    clean                   - Clean up working files." >&2
	echo "    resume <type>           - Resume from previous run." >&2
	echo "    stats                   - Show fuzzing stats." >&2
	echo "    package                 - Package results in ~/afl.zip." >&2
	echo ""  >&2
	echo "    base64                  - Fuzz base64 decoding." >&2
	echo "    certificate/certchain   - Fuzz certificate/cert.chain." >&2
	echo "    certreq                 - Fuzz cert.request." >&2
	echo "    cms/pgp                 - Fuzz CMS/PGP message." >&2
	echo "    pkcs12/pkcs15           - Fuzz PKCS #12/#15 keyset." >&2
	echo "    pgppub/pgpsec           - Fuzz PGP pub/priv.keyset." >&2
	echo "    ssl-client/ssl-server   - Fuzz SSL client/server." >&2
	echo "    ssh-client/ssh-server   - Fuzz SSH client/server." >&2
	echo "    tsp-client/tsp-server   - Fuzz TSP client/server." >&2
	echo "    ocsp-client/ocsp-server - Fuzz OCSP client/server." >&2
	echo "    rtcs-client/rtcs-server - Fuzz RTCS client/server." >&2
	echo "    (SCEP is fuzzed via CMS envelopes)." >&2
	echo "    http-req/http-resp      - Fuzz HTTP request/response." >&2
	exit 1
fi

# Make sure that we don't try and fuzz non-fuzzable protocols

if [ "$1" = "scep-client" -o "$1" = "scep-server" ] ; then
	echo "$0: SCEP uses CMS messages which are fuzzed via envelopes." >&2 ;
	exit 1
fi

# If we're doing a cleanup, delete input and output files and exit

if [ "$1" = "clean" ] ; then
	echo "Cleaning up..."
	shift
	if [ -z "$1" ] ; then
		echo "$0: Missing option to clean up." >&2
		exit 1
	fi
	rm -r ./${INPUT_DIR_PARENT}/$1
	rm -r ./${OUTPUT_DIR_PARENT}/$1
	exit 0
fi

# If we're showing stats, cat the fuzzer_stats files and exit

show_stats()
	{
	DIR=./${OUTPUT_DIR_PARENT}/$1

	if [ ! -d ${DIR} ] ; then
		return 0 ;
	fi
	echo Stats for $1...
	grep cycles_done ${DIR}/fuzzer_stats
	grep execs_done ${DIR}/fuzzer_stats
	grep execs_per_sec ${DIR}/fuzzer_stats
	grep unique_crashes ${DIR}/fuzzer_stats
	grep unique_hangs ${DIR}/fuzzer_stats
	echo
	}

if [ "$1" = "stats" ] ; then
	for FUZZTYPE in ${FUZZTYPES} ; do
		show_stats ${FUZZTYPE}
	done
	exit 0
fi

# If we're packaging up results, bundle everything up and exit

package()
	{
	DIR=./${OUTPUT_DIR_PARENT}/$1

	if [ ! -d ${DIR} ] ; then
		return 0
	fi
	mkdir /tmp/$1
	for file in ${DIR}/crashes/id* ; do
		outFile="${file#*:}"
		cp ${file} /tmp/$1/${outFile:0:6}.dat
	done
	cd /tmp
	zip -o9 ~/afl.zip ./$1/*
	cd -
	rm -r /tmp/$1
	}

if [ "$1" = "package" ] ; then
	rm ~/afl.zip
	for FUZZTYPE in ${FUZZTYPES} ; do
		package ${FUZZTYPE}
	done
	echo Results saved to ~/afl.zip
	exit 0
fi

# Create various directories

mkdir_opt()
	{
	DIR=$1
	OPTION=$2

	# Try and create the directory
	if [ ! -d ${DIR} ] ; then
		mkdir ${DIR}
		return 0
	fi
	if [ -z "$OPTION" ] ; then
		return 0
	fi

	# It already exists, handle it as per the caller's instructions
	case $OPTION in
		'clear-dup')
			echo "Warning: Input directory ${DIR} already exists, clearing files" ;
			rm ${DIR}/*.dat > /dev/null ;;

		'warn-dup')
			echo "Warning: Output directory ${DIR} already exists, were you" ;
			echo "         meaning to continue a previous run?" ;;

		*)
			echo "$0: Invalid mkdir_opt type $1." >&2 ;
			exit 1 ;;
	esac
	}

mkdir_opt ${INPUT_DIR_PARENT}
mkdir_opt ${OUTPUT_DIR_PARENT}

# If we're resuming from an aborted previous session, it's handled
# specially.

if [ "$1" = "resume" ] ; then
	shift
	if [ -z "$1" ] ; then
		echo "$0: Missing resume option.  Usage: '$0 resume <type>'" >&2
		exit 1
	fi
	echo "$0: Resuming fuzzing '$1' from previous session"
	FUZZTYPE=$1
	OUTPUT_DIR=${OUTPUT_DIR_PARENT}/$1
	nohup ${FUZZER} -m ${MEMORY_LIMIT} -i - -o ${OUTPUT_DIR} ${PROGNAME} -z${FUZZTYPE} @@ &
#	nohup ${FUZZER} -m ${MEMORY_LIMIT} -i - -o ${OUTPUT_DIR} ${PROGNAME} -z${FUZZTYPE} @@ > /dev/null 2>&1 &
	exit 0
fi

# Set up files and directories

FILEPATH="test/fuzz/"
case $1 in
	# Sessions reverse the argument, so to fuzz the xxx client we use
	# data from the xxx server.
	'ssh-client')
		FILENAME=${FILEPATH}ssh_svr.dat ;;

	'ssh-server')
		FILENAME=${FILEPATH}ssh_cli.dat ;;

	'ssl-client')
		FILENAME=${FILEPATH}ssl_svr.dat ;;

	'ssl-server')
		FILENAME=${FILEPATH}ssl_cli.dat ;;

	'tsp-client')
		FILENAME=${FILEPATH}tsp_svr.dat ;;

	'tsp-server')
		FILENAME=${FILEPATH}tsp_cli.dat ;;

	'ocsp-client')
		FILENAME=${FILEPATH}ocsp_svr.dat ;;

	'ocsp-server')
		FILENAME=${FILEPATH}ocsp_cli.dat ;;

	'rtcs-client')
		FILENAME=${FILEPATH}rtcs_svr.dat ;;

	'rtcs-server')
		FILENAME=${FILEPATH}rtcs_cli.dat ;;

	# HTTP uses underscores instead of dashes for the filename
	'http-req')
		FILENAME=${FILEPATH}http_req.dat ;;

	'http-resp')
		FILENAME=${FILEPATH}http_resp.dat ;;

	# Everything else uses the argument as the data source.
	*)
		FILENAME=${FILEPATH}$1.dat ;;
esac
if [ ! -f ${FILENAME} ] ; then
	echo "$0: Couldn't find data file ${FILENAME}." >&2
	exit 1
fi
mkdir_opt ${INPUT_DIR} clear-dup
mkdir_opt ${OUTPUT_DIR} warn-dup
cp ${FILENAME} ${INPUT_DIR}
if [ ! -x  ${PROGNAME} ] ; then
	cp ${PROGNAME_SRC} ${PROGNAME}
fi

# Run the fuzzer.  This takes files from ${INPUT_DIR} and pastes them into
# the '@@' location, with output in ${OUTPUT_DIR}/hangs and
# ${OUTPUT_DIR}/crashes.  If there are problems with timeouts, add something
# like -t 1000 (1s in ms).
#
# To start a single-instance fuzzer:
# nohup ${FUZZER} -m ${MEMORY_LIMIT} -i ${INPUT_DIR} -o ${OUTPUT_DIR} ${PROGNAME} -z${FUZZTYPE} @@ &
#
# This writes initial output to nohup.log, to make it completely silent
# append "> /dev/null 2>&1".
#
# In theory afl can run across multiple CPU cores with multiple instances, one
# per core, but this doesn't seem to work properly.  If it gets sorted out then
# the master is started with -M, the slaves with -S.

nohup ${FUZZER} -m ${MEMORY_LIMIT} -t 1000 -i ${INPUT_DIR} -o ${OUTPUT_DIR} ${PROGNAME} -z${FUZZTYPE} @@ &
exit 0

echo "Running on ${NO_CPUS} CPUs"
${FUZZER} -m ${MEMORY_LIMIT} -i ${INPUT_DIR} -o ${OUTPUT_DIR} -M fuzzer1 ${PROGNAME} -z${FUZZTYPE} @@ &

if [ $NO_CPUS -gt 1 ] ; then
	for i in `seq 2 $NUM_FUZZERS` ; do
		${FUZZER} -m ${MEMORY_LIMIT} -i ${INPUT_DIR} -o ${OUTPUT_DIR} -S fuzzer${i} ${PROGNAME} -z${FUZZTYPE} @@ &
	done
fi
