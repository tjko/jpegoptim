cd $SRC/jpegoptim

mkdir -p build

# Run all the builders
./libjpeg_builders/*.sh

# Build for libjpeg
cmake -S . -B build \
    -DBUILD_FUZZERS=On \
    -DCMAKE_C_COMPILER_WORKS=1 \
    -DUSE_MOZJPEG=0 \
    -DBUILD_SHARED_LIBS=OFF \
    -DLIBJPEG_INCLUDE_DIR=/usr/include
    -DLIBJPEG_LIBRARY=/usr/lib/x86_64-linux-gnu/libjpeg.a
  && cmake --build build --target install

# Build for libjpeg-turbo
cmake -S . -B build \
    -DBUILD_FUZZERS=On \
    -DCMAKE_C_COMPILER_WORKS=1 \
    -DUSE_MOZJPEG=0 \
    -DBUILD_SHARED_LIBS=OFF \
    -DLIBJPEG_INCLUDE_DIR=/usr/include
    -DLIBJPEG_LIBRARY=/usr/lib/x86_64-linux-gnu/libjpeg.a
  && cmake --build build --target install

# Build for mozjpeg
cmake -S . -B build \
    -DBUILD_FUZZERS=On \
    -DCMAKE_C_COMPILER_WORKS=1 \
    -DUSE_MOZJPEG=1 \
    -DBUILD_SHARED_LIBS=OFF
  && cmake --build build --target install

# Prepare corpora
mkdir -p fuzz/corpus
find . -name "*.jpg" -exec cp {} fuzz/corpus \;
zip -q $OUT/compression_fuzzer_seed_corpus.zip fuzz/corpus/*
