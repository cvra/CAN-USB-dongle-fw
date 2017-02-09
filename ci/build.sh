#!/usr/bin/env bash

# Make the script fail if any command in it fail
set -e

if [ "$BUILD_TYPE" == "" ]
then
    echo "Please define \$BUILD_TYPE!"
    exit 1
fi

export PATH=gcc-arm-none-eabi-4_9-2014q4/bin/:$PATH

export CFLAGS="$CFLAGS -I $HOME/cpputest/include/"
export CXXFLAGS="$CXXFLAGS -I $HOME/cpputest/include/"
export LDFLAGS="$CXXFLAGS -L $HOME/cpputest/lib/"

case $BUILD_TYPE in
    tests)
        mkdir build
        cd build
        cmake ../tests
        make check
        ;;

    build)
        make
        ;;
    *)
        echo "Unknown build type $BUILD_TYPE"
        exit 1
        ;;
esac
