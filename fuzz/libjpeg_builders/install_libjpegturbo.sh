#!/bin/bash

# This script builds libjpeg-turbo from source, as a static library
cd $SRC
git clone https://github.com/libjpeg-turbo/libjpeg-turbo.git
cd libjpeg-turbo
mkdir -p build
cmake -G"Unix Makefiles" -S . -B build -DBUILD_SHARED_LIBS=OFF -DWITH_JPEG8=1 -DCMAKE_INSTALL_PREFIX=/opt/libjpeg-turbo
cmake --build build --target install