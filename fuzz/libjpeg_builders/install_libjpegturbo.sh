#!/bin/bash

# This script builds libjpeg-turbo from source, as a static library
git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git
mkdir build
cd build
cmake -G"Unix Makefiles" -DBUILD_SHARED_LIBS=OFF
make -j`nproc`