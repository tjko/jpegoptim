cmake_minimum_required(VERSION 2.8)
project(jpegoptim)

#Dependency on libJPEG.
set(LIBJPEG_INCLUDE_DIR "" CACHE PATH "Path to directory containing header files of libJPEG (e.g. jpeglib.h).")
set(LIBJPEG_LIBRARY_DIR "" CACHE FILEPATH "Path to static library file of libJPEG (e.g. libjpeg.a).")
include_directories(${LIBJPEG_INCLUDE_DIR})
add_library(libjpeg STATIC IMPORTED)
set_target_properties(libjpeg PROPERTIES IMPORTED_LOCATION ${LIBJPEG_LIBRARY_DIR})

#Source files.
file(GLOB jpegoptim_SRC "*.c") #Adds all .c files in the source directory.

#Header files.
include_directories("${PROJECT_SOURCE_DIR}") #Adds all .h files in the source directory.

#Build the binary!
add_executable(jpegoptim ${jpegoptim_SRC})
target_link_libraries(jpegoptim libjpeg)