#!/bin/bash
find ./src -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" \) -a \! -path '*api*'  -a \! -path '*libespeak-ng*' -a \! -path '*libopus*' -a \! -path '*libtinysoundfont*' -exec astyle --suffix=none --options=./tests/astyle_core.conf \{\} \;
find ./examples -type f -name "*.ino" -exec astyle --suffix=none --options=./tests/astyle_examples.conf \{\} \;

