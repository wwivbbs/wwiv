#!/bin/bash
###############################################################
# use-built-bin.sh: Replaces file with symlink to the built
# version from cmake.
#
# BUILT_DIR_INCLUDING_WWIV: Example /home/rushfan/out/wwiv
# file: example: bbs
#
# Example use:
#
# ln -s $HOME/git/wwiv/builds/tools/linux/use-built-bin.sh
# export BUILT_BIN=$HOME/out/wwiv
# ./use-built-bin.sh ${BUILT_BIN} bbs wwivd wwivconfig network network?
#

if (( $# < 2 )); then
    echo "$0 BUILT_DIR_INCLUDING_WWIV file [file...]"
    exit 1
else
    BIN=$1
    shift
fi

for file in $@; do
    if [[ -x "$file" ]]; then
	echo "Linking file: $file"
	rm $file && ln -s ${BIN}/$file/$file
    else
	echo "$file was not an executable, skipping..."
    fi
done

