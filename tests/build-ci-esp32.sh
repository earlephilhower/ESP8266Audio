#!/bin/bash

# Called by CI to compile and dump an example
# built-ci.sh <FQBN> <output-directory> <full-path-to-ino>

# Ensure any failure == exit with error
set -e
set -o pipefail

echo "::group::Compiling $(basename $3) for $1 into $2"
./arduino-cli compile -b "$1" -v --warnings all --build-path "$2" "$3" || exit 255
mv -f "$2"/*.elf .
echo "::endgroup::"
