#!/bin/bash

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

