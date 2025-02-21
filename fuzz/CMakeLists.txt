# Fuzz process is dependent upon a few environment variables provided by OSSFuzz during the build process
# For more information, see google.github.io/oss-fuzz/getting-started/new-project-guide/#buildsh-script-environment

add_definitions(-DNDEBUG)  # Do not want assertions for fuzz-testing

add_executable(compression_fuzzer fuzz_compression.c fuzz_manager.c)

# Configure compilation options
target_compile_options(compression_fuzzer PRIVATE "$ENV{CMAKE_C_FLAGS}")
set(CMAKE_C_COMPILER "$ENV{CC}" CACHE STRING "C compiler" FORCE)

# Configure linking options
target_link_options(compression_fuzzer PRIVATE "$ENV{LIB_FUZZING_ENGINE}")
target_link_libraries(compression_fuzzer PRIVATE ${PROJECT_NAME})

# Install the built fuzzer(s) to output directory
if (DEFINED ENV{OUT})
    install(TARGETS compression_fuzzer DESTINATION $ENV{OUT})
else()
    message(FATAL_ERROR "Cannot install if $OUT is not defined!")
endif()