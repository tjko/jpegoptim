cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

project(jpegoptim C)

# LIBJPEG_INCLUDE_DIR and LIBJPEG_LIBRARY must both be specified if a custom libjpeg implementation is desired.
option(WITH_ARITH "Enable arithmetic coding (if supported by the libjpeg implementation)" 1)
option(USE_MOZJPEG "Download, build, and link with MozJPEG rather than the system libjpeg. Build with NASM installed for SIMD support." 1)
set(LIBJPEG_INCLUDE_DIR "" CACHE PATH "Custom libjpeg header directory")
set(LIBJPEG_LIBRARY "" CACHE FILEPATH "Custom libjpeg library binary")
if(MSVC)
    option(BUILD_NO_SUBFOLDERS "Flatten the compiled program's output path")
endif()

# If LIBJPEG_INCLUDE_DIR and LIBJPEG_LIBRARY are set, USE_MOZJPEG is disabled.
if(LIBJPEG_INCLUDE_DIR AND LIBJPEG_LIBRARY)
    set(USE_MOZJPEG 0)
endif()

# Set target architecture if empty. CMake's Visual Studio generator provides it, but others may not.

if(MSVC)
    if(NOT CMAKE_VS_PLATFORM_NAME)
        set(CMAKE_VS_PLATFORM_NAME "x64")
    endif()
    message("${CMAKE_VS_PLATFORM_NAME} architecture in use")
else()
    add_compile_definitions(HOST_TYPE="${CMAKE_HOST_SYSTEM_NAME}")
endif()


# Global configuration types

set(CMAKE_CONFIGURATION_TYPES
    "Debug"
    "Release"
    CACHE STRING "" FORCE
)


# Global compiler options

if(MSVC)
    # remove default compiler flags provided with CMake for MSVC
    set(CMAKE_C_FLAGS "")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_C_FLAGS_RELEASE "")
endif()


# Global linker options

if(MSVC)
    # remove default linker flags provided with CMake for MSVC
    set(CMAKE_EXE_LINKER_FLAGS "")
    set(CMAKE_MODULE_LINKER_FLAGS "")
    set(CMAKE_SHARED_LINKER_FLAGS "")
    set(CMAKE_STATIC_LINKER_FLAGS "")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS}")
endif()


# Common utils

include(CMake/Utils.cmake)


# Additional Global Settings (add specific info there)

include(CMake/GlobalSettingsInclude.cmake OPTIONAL)


# Use solution folders feature

set_property(GLOBAL PROPERTY USE_FOLDERS ON)


# Source groups

file(GLOB SOURCE_FILES *.c)
source_group("Source Files" FILES ${SOURCE_FILES})


# Target

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

if(MSVC)
    use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_GLOBAL_KEYWORD "Win32Proj"
    )
endif()


# Output directory

if(MSVC)
    if(BUILD_NO_SUBFOLDERS)
        set(BINARY_OUTPUT_PATH ".")
    else()
        set(BINARY_OUTPUT_PATH "$<CONFIG>/${CMAKE_VS_PLATFORM_NAME}")
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BINARY_OUTPUT_PATH}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BINARY_OUTPUT_PATH}
    )
endif()


# Interprocedural optimization (LTCG)

set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PUBLIC
        -fuse-linker-plugin
    )
endif()


# MSVC runtime library

if(MSVC)
    get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
    string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
        $<$<CONFIG:Debug>:
            MultiThreadedDebug
        >
        $<$<CONFIG:Release>:
            MultiThreaded
        >
        $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})
endif()


# Include directories

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)


# Compile definitions

target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:"
        "_DEBUG;"
        "DEBUG"
    ">"
    "$<$<CONFIG:Release>:"
        "NDEBUG"
    ">"
)

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "WIN32;"
        "_WIN64;"
        "WIN64;"
        "_WINDOWS;"
        "UNICODE;"
        "_UNICODE"
    )
endif()


# Compile and link options

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /Od;             # Disable optimization
            /RTC1            # Enable stack frame run-time error checking and reporting when a variable is used without having been initialized.
        >
        $<$<CONFIG:Release>:
            /MP;             # Build with multiple processes
            /O2;             # Optimize for speed
            /GF              # Enable string pooling
        >
        /Gy;                 # Link per-function
        /W3;                 # Warning level
        /Zi;                 # Emit debug info in a separate PDB
        /TC;                 # Compile all source files as C source code regardless of extension
        /wd4996;             # Suppress deprecation warnings
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /GS;                 # Enable security checks against buffer overruns
        /Y-                  # Disable precompiled headers
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:
            /INCREMENTAL     # Enable incremental linking (faster builds, larger filesize)
        >
        $<$<CONFIG:Release>:
            /OPT:REF;        # Don't link unused functions
            /OPT:ICF;        # Remove duplicate function definitions
            /INCREMENTAL:NO  # Disable incremental linking
        >
        /MANIFEST;           # Generate a manifest file
        /DEBUG:FULL;         # Generate debugging symbols (in a separate PDB file)
        /MACHINE:${CMAKE_VS_PLATFORM_NAME};
        /SUBSYSTEM:CONSOLE;  # Not a driver or GUI program
        /NXCOMPAT;           # Support Windows Data Execution Prevention
        /DYNAMICBASE         # Use address space layout randomization
    )

    # Link with setargv for command line wildcard support
    # See https://learn.microsoft.com/en-us/cpp/c-language/expanding-wildcard-arguments

    target_link_options(${PROJECT_NAME} PRIVATE
        setargv.obj
    )
endif()


# Header and function checks

include(CheckIncludeFile)
check_include_file(config.h HAVE_CONFIG_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(getopt.h HAVE_GETOPT_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(libgen.h HAVE_LIBGEN_H)
check_include_file(math.h HAVE_MATH_H)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(dirent.h HAVE_DIRENT_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/wait.h HAVE_SYS_WAIT_H)

include(CheckSymbolExists)
check_symbol_exists(mkstemps "stdlib.h" HAVE_MKSTEMPS)
check_symbol_exists(labs "stdlib.h" HAVE_LABS)
check_symbol_exists(fileno "stdio.h" HAVE_FILENO)
check_symbol_exists(utimensat "sys/stat.h" HAVE_UTIMENSAT)
check_symbol_exists(fork "unistd.h" HAVE_FORK)
check_symbol_exists(wait "sys/wait.h" HAVE_WAIT)
check_symbol_exists(getopt "unistd.h" HAVE_GETOPT)
check_symbol_exists(getopt_long "getopt.h" HAVE_GETOPT_LONG)

include(CheckStructHasMember)

if(HAVE_SYS_STAT_H)
    check_struct_has_member(
            "struct stat" st_mtim "sys/stat.h" HAVE_STRUCT_STAT_ST_MTIM LANGUAGE C
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<BOOL:${HAVE_CONFIG_H}>:HAVE_CONFIG_H>
    $<$<BOOL:${HAVE_UNISTD_H}>:HAVE_UNISTD_H>
    $<$<BOOL:${HAVE_GETOPT_H}>:HAVE_GETOPT_H>
    $<$<BOOL:${HAVE_STRING_H}>:HAVE_STRING_H>
    $<$<BOOL:${HAVE_LIBGEN_H}>:HAVE_LIBGEN_H>
    $<$<BOOL:${HAVE_MATH_H}>:HAVE_MATH_H>
    $<$<BOOL:${HAVE_FCNTL_H}>:HAVE_FCNTL_H>
    $<$<BOOL:${HAVE_DIRENT_H}>:HAVE_DIRENT_H>
    $<$<BOOL:${HAVE_SYS_STAT_H}>:HAVE_SYS_STAT_H>
    $<$<BOOL:${HAVE_SYS_TYPES_H}>:HAVE_SYS_TYPES_H>
    $<$<BOOL:${HAVE_SYS_WAIT_H}>:HAVE_SYS_WAIT_H>
    $<$<BOOL:${HAVE_MKSTEMPS}>:HAVE_MKSTEMPS>
    $<$<BOOL:${HAVE_LABS}>:HAVE_LABS>
    $<$<BOOL:${HAVE_FILENO}>:HAVE_FILENO>
    $<$<BOOL:${HAVE_UTIMENSAT}>:HAVE_UTIMENSAT>
    $<$<BOOL:${HAVE_FORK}>:HAVE_FORK>
    $<$<BOOL:${HAVE_WAIT}>:HAVE_WAIT>
    $<$<BOOL:${HAVE_GETOPT}>:HAVE_GETOPT>
    $<$<BOOL:${HAVE_GETOPT_LONG}>:HAVE_GETOPT_LONG>
    $<$<BOOL:${HAVE_STRUCT_STAT_ST_MTIM}>:HAVE_STRUCT_STAT_ST_MTIM>
)


# Attach a manifest file to support UTF-8 on compatible Windows systems (see https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page)

if(MSVC)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND "mt.exe" -nologo -manifest \"${CMAKE_CURRENT_SOURCE_DIR}/jpegoptim.manifest\" -outputresource:"${CMAKE_CURRENT_BINARY_DIR}/${BINARY_OUTPUT_PATH}/jpegoptim.exe"\;\#1
        COMMENT "Adding manifest..."
    )
endif()


# Dependencies

if(USE_MOZJPEG)
    # Link with mozjpeg.
    # Version tree: https://github.com/mozilla/mozjpeg/tree/fd569212597dcc249752bd38ea58a4e2072da24f

    include(ExternalProject)

    if(WITH_ARITH)
        set(ARITH_FLAGS -DWITH_ARITH_DEC=1 -DWITH_ARITH_ENC=1)
        set(JPEGLIB_SUPPORTS_ARITH_CODE 1)
    endif()

    ExternalProject_Add(mozjpeg_lib
         GIT_REPOSITORY https://github.com/mozilla/mozjpeg.git
         GIT_TAG fd569212597dcc249752bd38ea58a4e2072da24f
         PREFIX ${CMAKE_CURRENT_BINARY_DIR}/mozjpeg
         CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/mozjpeg -DPNG_SUPPORTED=0 -DWITH_TURBOJPEG=0 -DENABLE_SHARED=0 ${ARITH_FLAGS}
    )


    # Building and linking mozjpeg as a library, as explained here https://mirkokiefer.com/cmake-by-example-f95eb47d45b1

    ExternalProject_Get_Property(mozjpeg_lib install_dir)
    add_library(mozjpeg STATIC IMPORTED)
    if(MSVC)
        set_property(TARGET mozjpeg PROPERTY IMPORTED_LOCATION ${install_dir}/lib/jpeg-static.lib)
    else()
        target_link_libraries(mozjpeg INTERFACE m)
        set_property(TARGET mozjpeg PROPERTY IMPORTED_LOCATION ${install_dir}/lib/libjpeg.a)
    endif()
    add_dependencies(mozjpeg mozjpeg_lib)
    target_include_directories(${PROJECT_NAME} BEFORE PRIVATE ${install_dir}/include)
    target_link_libraries(${PROJECT_NAME} mozjpeg)
    add_dependencies(${PROJECT_NAME} mozjpeg)

    # Note: check_include_file, check_symbol_exists, check_struct_has_member, and check_c_source_compiles
    # cannot be used on ExternalProject dependencies because they are not compiled until build time, while check_*
    # functions run during the initial CMake configuration step.
    # Since the version is hardcoded above, feature checks may be set as constants as a workaround.

    set(HAVE_JINT_DC_SCAN_OPT_MODE 1)

else()
    if(LIBJPEG_INCLUDE_DIR AND LIBJPEG_LIBRARY)
        # Link with custom libjpeg
        add_library(libjpeg STATIC IMPORTED)
        if(NOT MSVC)
            target_link_libraries(libjpeg INTERFACE m)
        endif()
        set_target_properties(libjpeg PROPERTIES IMPORTED_LOCATION ${LIBJPEG_LIBRARY})
        target_include_directories(${PROJECT_NAME} BEFORE PRIVATE ${LIBJPEG_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} libjpeg)
        add_dependencies(${PROJECT_NAME} libjpeg)
    else()
        # Link with system libjpeg
        include(FindJPEG)
        if(NOT JPEG_FOUND)
            message(FATAL_ERROR "Could not automatically locate libjpeg. Either specify -DUSE_MOZJPEG=1 to download and build with MozJPEG, or -DLIBJPEG_INCLUDE_DIR=... and -DLIBJPEG_LIBRARY=... to the appropriate paths to build with a custom libjpeg implementation.")
        endif()
        message(STATUS "Include dirs: ${JPEG_INCLUDE_DIRS}")
        target_include_directories(${PROJECT_NAME} PRIVATE ${JPEG_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} JPEG::JPEG)
    endif()

    # Use all include directories and linked libraries as the main project for feature tests
    get_target_property(CMAKE_REQUIRED_LIBRARIES ${PROJECT_NAME} LINK_LIBRARIES)
    get_target_property(CMAKE_REQUIRED_INCLUDES ${PROJECT_NAME} INCLUDE_DIRECTORIES)

    # check_include_file, check_symbol_exists, and check_struct_has_member cannot be used with libjpeg.h
    # because libjpeg.h requires stdio.h to be included before it to not throw an unrelated compilation error.

    include(CheckCSourceCompiles)
    check_c_source_compiles(
            "
            #include <stdio.h>
            #include <jpeglib.h>
            int main(void)
            {
              return sizeof (&jpeg_read_header);
            }
            "
            HAVE_APPROPRIATE_LIBJPEG_VERSION
    )

    if(NOT HAVE_APPROPRIATE_LIBJPEG_VERSION)
        message(FATAL_ERROR "Invalid version: libjpeg version 6 or later is required.")
    endif()

    check_c_source_compiles(
            "
            #include <stdio.h>
            #include <jpeglib.h>
            METHODDEF(void) foo(void) {};
            int main(void)
            {
              return 0;
            }
            "
            WORKING_METHODDEF
    )

    if(NOT WORKING_METHODDEF)
        target_compile_definitions(${PROJECT_NAME} PRIVATE -DBROKEN_METHODDEF)
    endif()

    if(WITH_ARITH)
        # Check for arithmetic coding support

        check_c_source_compiles(
                "
                #include <stdio.h>
                #include <jpeglib.h>
                int main(void)
                {
                    return sizeof (((struct jpeg_compress_struct *)0)->arith_code);
                }
                "
                JPEGLIB_SUPPORTS_ARITH_CODE
        )
    endif()

    # Check for MozJPEG's JINT_DC_SCAN_OPT_MODE extension

    check_c_source_compiles(
            "
            #include <stdio.h>
            #include <jpeglib.h>
            int main(void)
            {
                struct jpeg_compress_struct cinfo;
                if (jpeg_c_int_param_supported(&cinfo, JINT_DC_SCAN_OPT_MODE))
                    jpeg_c_set_int_param(&cinfo, JINT_DC_SCAN_OPT_MODE, 1);
                return 0;
            }
            "
            HAVE_JINT_DC_SCAN_OPT_MODE
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
        $<$<BOOL:${HAVE_JINT_DC_SCAN_OPT_MODE}>:HAVE_JINT_DC_SCAN_OPT_MODE>
)


if(WITH_ARITH AND JPEGLIB_SUPPORTS_ARITH_CODE)
    set(ARITH_ENABLED 1)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAVE_ARITH_CODE)
endif()

if(ARITH_ENABLED)
    message(STATUS "Arithmetic Coding: Enabled")
else()
    message(STATUS "Arithmetic Coding: Disabled")
endif()
