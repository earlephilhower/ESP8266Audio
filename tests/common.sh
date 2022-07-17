#!/usr/bin/env bash

set -ex

function install_libraries()
{
    mkdir -p $HOME/Arduino/libraries
    cp -a $GITHUB_WORKSPACE $HOME/Arduino/libraries/ESP8266Audio
    git clone https://github.com/earlephilhower/ESP8266SAM $HOME/Arduino/libraries/ESP8266SAM
}

function install_ide()
{
    local ide_path=$1
    wget -q -O arduino.tar.xz https://downloads.arduino.cc/arduino-nightly-linux64.tar.xz
    tar xf arduino.tar.xz
    mv arduino-nightly $ide_path
    export PATH="$ide_path:$PATH"
}

function install_esp8266()
{
    local ide_path=$1
    mkdir -p $ide_path/hardware
    cd $ide_path/hardware
    mkdir esp8266com
    cd esp8266com
    git clone https://github.com/esp8266/Arduino esp8266
    pushd esp8266/tools
    # Set custom warnings for all builds (i.e. could add -Wextra at some point)
    echo "compiler.c.extra_flags=-Wall -Wextra -Werror $debug_flags" > ../platform.local.txt
    echo "compiler.cpp.extra_flags=-Wall -Wextra -Werror $debug_flags" >> ../platform.local.txt
    echo "mkbuildoptglobals.extra_flags=--ci --cache_core" >> ../platform.local.txt
    echo -e "\n----platform.local.txt----"
    cat ../platform.local.txt
    git submodule init
    git submodule update
    python3 get.py -q
    export PATH="$ide_path/hardware/esp8266com/esp8266/tools/xtensa-lx106-elf/bin:$PATH"
    popd
    cd esp8266
}

function install_rp2040()
{
    local ide_path=$1
    mkdir -p $ide_path/hardware
    cd $ide_path/hardware
    mkdir pico
    cd pico
    git clone https://github.com/earlephilhower/arduino-pico rp2040
    pushd rp2040/tools
    # Set custom warnings for all builds (i.e. could add -Wextra at some point)
    echo "compiler.c.extra_flags=-Wall -Wextra -Werror $debug_flags" > ../platform.local.txt
    echo "compiler.cpp.extra_flags=-Wall -Wextra -Werror $debug_flags" >> ../platform.local.txt
    echo -e "\n----platform.local.txt----"
    cat ../platform.local.txt
    git submodule update --init
    cd ../pico-sdk
    git submodule update --init
    cd ../tools
    python3 get.py -q
    export PATH="$ide_path/hardware/pico/rp2040/system/arm-none-eabi/bin:$PATH"
    popd
    cd rp2040
}

function install_esp32()
{
    local ide_path=$1
    pip install pyserial
    pip3 install pyserial
    cd $ide_path/hardware
    mkdir espressif
    cd espressif
    git clone https://github.com/espressif/arduino-esp32.git esp32
    pushd esp32
    # Set custom warnings for all builds

    echo "compiler.c.extra_flags=-Wall -Wextra -Werror $debug_flags" > platform.local.txt
    echo "compiler.cpp.extra_flags=-Wall -Wextra -Werror $debug_flags" >> platform.local.txt
    echo -e "\n----platform.local.txt----"
    cat platform.local.txt
    git submodule update --init
    cd tools
    python3 get.py
    export PATH="$ide_path/hardware/espressif/esp32/tools/xtensa-esp32-elf/bin/:$PATH"
    popd
    cd esp32
}

function install_arduino()
{
    # Install Arduino IDE and required libraries
    cd "$GITHUB_WORKSPACE"
    install_ide "$HOME/arduino_ide" "$GITHUB_WORKSPACE"
    which arduino
    cd "$GITHUB_WORKSPACE"
    install_libraries
}

if [ "$BUILD_MOD" == "" ]; then
    export BUILD_MOD=1
    export BUILD_REM=0
fi

export cache_dir=$(mktemp -d)
if [ "$BUILD_TYPE" = "build" ]; then
    install_arduino
    install_esp8266 "$HOME/arduino_ide"
    source "$HOME/arduino_ide/hardware/esp8266com/esp8266/tests/common.sh"
    build_sketches "$HOME/arduino_ide" "$TRAVIS_BUILD_DIR" "-l $HOME/Arduino/libraries" "$BUILD_MOD" "$BUILD_REM" "lm2f"
elif [ "$BUILD_TYPE" = "build_esp32" ]; then
    install_arduino
    install_esp32 "$HOME/arduino_ide"
    export FQBN="espressif:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app"
    mkdir -p "$GITHUB_WORKSPACE/hardware"
    ln -s "$GITHUB_WORKSPACE/../" "$GITHUB_WORKSPACE/libraries"
    source "$HOME/arduino_ide/hardware/espressif/esp32/.github/scripts/sketch_utils.sh" chunk_build -ai "$HOME/arduino_ide" -au "$GITHUB_WORKSPACE" -fqbn "$FQBN" -t esp32 -p "$GITHUB_WORKSPACE" -i $BUILD_REM -m $BUILD_MOD
elif [ "$BUILD_TYPE" = "build_rp2040" ]; then
    install_arduino
    install_rp2040 "$HOME/arduino_ide"
    source "$HOME/arduino_ide/hardware/pico/rp2040/tests/common.sh"
    build_sketches "$HOME/arduino_ide" "$TRAVIS_BUILD_DIR" "-l $HOME/Arduino/libraries" "$BUILD_MOD" "$BUILD_REM"
fi
