#!/bin/sh
for file in include/libdset/*.h; do
    case $file in
    */ui.h) continue ;;
    esac
    grep ^extern $file | sed -r -e 's/\(.*//' -e 's/.* \*?//' | egrep -v '\[|\;'
done | while read symbol; do
    # echo $symbol
    if [ -z "$symbol" -o "$symbol" = '{' ]; then
    	continue
    fi
    if [ -z "`grep \" $symbol\;\" lib/libdset.map`" ]; then
    	echo "Symbol $symbol is missing from libdset.map"
    fi
done
