#!/bin/bash

set -e
set -o pipefail

outdir=$1; shift
offset=$1; shift
fqbn=$1; shift
werror=$1; shift

mkdir $outdir
rm -f *.elf

# Get initial list and shift off to the starting point
sketches=$(find ~/Arduino/libraries/ESP8266Audio/examples -name "*.ino" | sort | tr '\n' ' ' | cut -f$offset -d " ")

# Iterate over sketches
while [ $(echo $sketches | wc -w) -gt 0 ]; do
    sketch=$(echo $sketches | cut -f1 -d " ")
    echo "::group::Compiling $(basename $sketch) for $fqbn into $outdir"
    if [ 0"$werror" -gt 0 ]; then
        ./arduino-cli compile -b "$fqbn" -v --warnings all \
            --build-property "compiler.c.extra_flags=-Wall -Wextra -Werror -Wno-ignored-qualifiers" \
            --build-property "compiler.cpp.extra_flags=-Wall -Wextra -Werror -Wno-ignored-qualifiers -Wno-overloaded-virtual" \
            --build-path "$outdir" "$sketch" || exit 255
   else
       ./arduino-cli compile -b "$fqbn" -v --warnings all --build-path "$outdir" "$sketch" || exit 255
   fi
    mv -f "$outdir"/*.elf .
    echo "::endgroup::"
    # Shift out 5
    if [  $(echo $sketches | wc -w) -gt 5 ]; then
        sketches=$(echo $sketches | cut -f6- -d " ")
    else
        sketches=""
    fi
done

echo "::group::Final Sizes"
size ./*.elf
echo "::endgroup::"
