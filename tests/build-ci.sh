#!/bin/bash

set -e
set -o pipefail

outdir=$1; shift
offset=$1; shift
fqbn=$1; shift

mkdir $outdir
rm -f *.elf

# Get initial list and shift off to the starting point
sketches=$(find ~/Arduino/libraries/ESP8266Audio/examples -name "*.ino" | sort | tr '\n' ' ' | cut -f$offset -d " ")

# Iterate over sketches
while [ $(echo $sketches | wc -w) -gt 0 ]; do
    sketch=$(echo $sketches | cut -f1 -d " ")
    echo "::group::Compiling $(basename $sketch) for $fqbn into $outdir"
    ./arduino-cli compile -b "$fqbn" -v --warnings all --build-path "$outdir" "$sketch" || exit 255
    mv -f "$outdir"/*.elf .
    echo "::endgroup::"
    # Shift out 5
    sketches=$(echo $sketches | cut -f6- -d " ")
done

echo "::group::Final Sizes"
size ./*.elf
echo "::endgroup::"
