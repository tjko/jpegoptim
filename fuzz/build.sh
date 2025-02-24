cd $SRC/jpegoptim

mkdir -p build

export ASAN_OPTIONS=detect_leaks=0

# Build for libjpeg
cmake -S . -B build-libjpeg \
    -DJPEG_ENGINE=libjpeg \
    -DBUILD_FUZZERS=On \
    -DCMAKE_C_COMPILER_WORKS=1 \
    -DUSE_MOZJPEG=0 \
    -DBUILD_SHARED_LIBS=OFF \
    -DLIBJPEG_INCLUDE_DIR=/usr/include \
    -DLIBJPEG_LIBRARY=/usr/lib/x86_64-linux-gnu/libjpeg.a \
  && cmake --build build-libjpeg --target install

# Build for libjpeg-turbo
cmake -S . -B build-libjpegturbo \
    -DJPEG_ENGINE=libjpegturbo \
    -DBUILD_FUZZERS=ON \
    -DCMAKE_C_COMPILER_WORKS=1 \
    -DUSE_MOZJPEG=0 \
    -DBUILD_SHARED_LIBS=OFF \
    -DLIBJPEG_INCLUDE_DIR=/opt/libjpeg-turbo/include \
    -DLIBJPEG_LIBRARY=/opt/libjpeg-turbo/lib64/libjpeg.a \
  && cmake --build build-libjpegturbo --target install

# Build for mozjpeg
cmake -S . -B build-mozjpeg \
    -DJPEG_ENGINE=mozjpeg \
    -DBUILD_FUZZERS=On \
    -DCMAKE_C_COMPILER_WORKS=1 \
    -DUSE_MOZJPEG=1 \
    -DBUILD_SHARED_LIBS=OFF \
  && cmake --build build-mozjpeg --target install

# Prepare corpora
mkdir -p fuzz/corpus
find . -name "*.jpg" -exec cp {} fuzz/corpus \;

# Save corpora per build
zip -q $OUT/libjpeg_compression_fuzzer_seed_corpus.zip fuzz/corpus/*
cp $OUT/libjpeg_compression_fuzzer_seed_corpus.zip $OUT/libjpegturbo_compression_fuzzer_seed_corpus.zip
cp $OUT/libjpeg_compression_fuzzer_seed_corpus.zip $OUT/mozjpeg_compression_fuzzer_seed_corpus.zip
