#!/bin/bash
echo "Content-type: text/xml\n\n"
echo ""

curl "http://sourceforge.net/p/wwiv/bugs/feed.rss"

