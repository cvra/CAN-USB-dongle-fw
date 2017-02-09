#!/usr/bin/env bash

# Make the script fail if any command in it fail
set -e

if [ "$BUILD_TYPE" == "build" ]
then
    # Install ARM GCC
    wget https://launchpad.net/gcc-arm-embedded/4.9/4.9-2014-q4-major/+download/gcc-arm-none-eabi-4_9-2014q4-20141203-linux.tar.bz2
    tar -xf gcc-arm-none-eabi-4_9-2014q4-20141203-linux.tar.bz2

else
    if [ "$BUILD_TYPE" == "tests" ]
    then
        # Install cpputest from source
        pushd ..
        wget "https://github.com/cpputest/cpputest.github.io/blob/master/releases/cpputest-3.8.tar.gz?raw=true" -O cpputest.tar.gz
        tar -xzf cpputest.tar.gz
        cd cpputest-3.6/
        ./configure --prefix=$HOME/cpputest
        make
        make install
        popd
    fi
fi
