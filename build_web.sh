#!/bin/bash -e

# We need the SDK to setup an emscripten build.
if [ $# -ne 1 ]; then
	echo "Usage:"
	echo "  $0 <sdk>"
	echo "Where <sdk> is the path to the emscripten SDK folder"
	exit 1
fi
SDK=$1

# Check the SDK is valid.
if [ ! -f "${SDK}/emsdk_env.sh" ]; then
	echo "Incorrect SDK path"
	exit 1
fi

# Activate the SDK in the current shell.
echo "Activating SDK..."
source "${SDK}/emsdk_env.sh"

# Check that emcmake can be found.
echo "Checking for emcmake..."
echo "If this fails then make sure that you've installed and activated the SDK."
command -v emcmake

# Download SDL3.
touch tmp.c
emcc -sUSE_SDL=3 -c tmp.c -o tmp.o
rm tmp.c tmp.o

# Do the builds.
cmake -S . -B out \
    && cmake --build out --parallel
emcmake cmake -S . -B out_web -D CMAKE_BUILD_TYPE=Release \
    && cmake --build out_web --parallel
