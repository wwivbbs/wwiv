#!/bin/sh
# Move all OpenSSL symbols into their own namespace to avoid conflicts if
# linking with both cryptlib and OpenSSL.  The Mac version of sed is somewhat
# special and needs its own custom invocation.  In addition since a few of
# the test files have non-ASCII character sets and that also upsets the Mac
# sed, we have to switch to 8859-1 for those.  We use en_US because it's
# likely to be present on most systems.

replace_patterns() {
    echo $1
    if file $1 | grep --quiet "ISO-8859"; then
        if [ "$(uname)" = "Darwin" ] ; then
            LANG=en_US.iso88591 sed -i '' -f tools/patterns.sed $1
        else
            LANG=en_US.iso88591 sed -i -f tools/patterns.sed $1
        fi
    else
        if [ "$(uname)" = "Darwin" ] ; then
            LANG=C sed -i '' -f tools/patterns.sed $1
        else
            LANG=C sed -i -f tools/patterns.sed $1
        fi
    fi
}

export -f replace_patterns

find ./ -name '*.h' -type f -exec bash -c 'replace_patterns "$0"' {} \;
find ./ -name '*.c' -type f -exec bash -c 'replace_patterns "$0"' {} \;
