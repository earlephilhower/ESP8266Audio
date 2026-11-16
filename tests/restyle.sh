#!/bin/bash
base=$(git rev-parse --show-toplevel)
cd "$base/tests"

# Build our own astyle because version creep could cause issues
if ! [ -e astyle ]; then
    git clone https://gitlab.com/saalen/astyle.git astyle
    cd astyle
    git checkout 3.6.20
    cd AStyle
    mkdir build
    cd build/
    cmake ..
    make -j
fi
astyle="$base/tests/astyle/AStyle/build/astyle"

find $base/src -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" \) -a \! -path '*api*'  -a \! -path '*libespeak-ng*' -a \! -path '*libopus*' -a \! -path '*libtinysoundfont*' -exec $astyle --suffix=none --options=$base/tests/astyle_core.conf \{\} \;
find $base/examples -type f -name "*.ino" -exec $astyle --suffix=none --options=$base/tests/astyle_examples.conf \{\} \;

