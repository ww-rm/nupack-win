cmake_minimum_required(VERSION 3.10)
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "Do not build in-source. Please remove CMakeCache.txt and the CMakeFiles/ directory. Then build out-of-source.")
endif()

include_directories(
    "/d/Projects/VsProjects/nupack-4.0.1.8/source/external/vcpkg/installed/x64-mingw-dynamic/include/libsimdpp-2.1"
)

link_directories(
    "/d/CondaEnvs/py39/libs"
    # "/d/CondaEnvs/py310/libs"
    # "/d/CondaEnvs/py311/libs"
    # "/d/CondaEnvs/py312/libs"
)

link_libraries(
    imagehlp

    python39
    # python310
    # python311
    # python312
)

################################################################################

if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
endif()
if(POLICY CMP0051)
    cmake_policy(SET CMP0051 NEW)
endif()
if(POLICY CMP0058)
    cmake_policy(SET CMP0058 NEW)
endif()
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0054 NEW)

################################################################################

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake
  CACHE STRING "Vcpkg toolchain file")

################################################################################

set(CMAKE_C_COMPILER_NAMES gcc clang icc cc)
set(CMAKE_CXX_COMPILER_NAMES clang gcc icc cc)

enable_language(C)
enable_language(CXX)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(Contents)
include(Options)

################################################################################
project(NUPACK VERSION 4.0.1.8 LANGUAGES CXX C)
################################################################################

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE Release)
endif()

message("-- CMake build type is \"${CMAKE_BUILD_TYPE}\"")

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)

################################################################################

# This function preferentially symlinks a file and records the output in ${link_outputs}
# If on Windows, a copy is done instead of symlinking
function(link_file in out)
    get_filename_component(dir "${out}" DIRECTORY)
    if(WIN32)
        set(link_file_flag "copy")
    else()
        set(link_file_flag "create_symlink")
    endif()
    add_custom_command(OUTPUT "${out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${dir}
        COMMAND ${CMAKE_COMMAND} -E ${link_file_flag} "${in}" "${out}"
        DEPENDS "${in}"
    )
    set(link_outputs ${link_outputs} "${out}" PARENT_SCOPE)
endfunction(link_file)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/solver)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/rebind EXCLUDE_FROM_ALL)

################################################################################

if(NUPACK_BUILD_CXX)
    include(BuildCXX)
endif()

if(NUPACK_BUILD_PYTHON)
    include(BuildPython)
endif()

if(NUPACK_BUILD_DOCS)
    include(BuildDocs)
endif()

################################################################################
