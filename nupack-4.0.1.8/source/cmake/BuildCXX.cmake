################################################################################

include(ExternalProject)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(GNUInstallDirs)
include(Libraries)

################################################################################

cmake_policy(SET CMP0042 NEW)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0051 NEW)
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0058 NEW)

################################################################################

message("-- Building project NUPACK ${NUPACK_VERSION}")

include(${CMAKE_CURRENT_SOURCE_DIR}/external/cmake-modules/GetGitRevisionDescription.cmake)
get_git_head_revision(NUPACK_GIT_BRANCH NUPACK_GIT_REVISION)
if(NUPACK_GIT_BRANCH)
    message("-- On deduced git branch \"${NUPACK_GIT_BRANCH}\", revision \"${NUPACK_GIT_REVISION}\"")
elseif(DEFINED ENV{NUPACK_GIT_BRANCH} AND DEFINED ENV{NUPACK_GIT_REVISION})
    set(NUPACK_GIT_BRANCH $ENV{NUPACK_GIT_BRANCH})
    set(NUPACK_GIT_REVISION $ENV{NUPACK_GIT_REVISION})
    message("-- On preset git branch \"${NUPACK_GIT_BRANCH}\", revision \"${NUPACK_GIT_REVISION}\"")
else()
    message("-- Could not detect git information")
endif()

################################################################################

# set(CMAKE_SKIP_RPATH TRUE)
# set(CMAKE_CXX_VISIBILITY_PRESET hidden) do this for Python and libnupack.a in future?

################################################################################

add_library(cxxflags INTERFACE)
target_compile_options(cxxflags INTERFACE $<$<CONFIG:Release>:-O3 -fomit-frame-pointer>)
# Be very wary of -ofast. Our code depends on proper treatment of NaN and inf!

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    message("-- Using \"AppleClang\" compiler flags")
    target_compile_options(cxxflags INTERFACE -ferror-limit=4 -fdiagnostics-color=always -ftemplate-depth=1024 -stdlib=libc++)
    set(exeflags "-mmacosx-version-min=10.8")

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    message("-- Using \"Clang\" compiler flags")
    target_compile_options(cxxflags INTERFACE
        ${NUPACK_SIMD_FLAGS} -ftemplate-backtrace-limit=0 -fdiagnostics-color=always -ftemplate-depth=1024 -Wpessimizing-move -Wno-c++20-designator
        -ftemplate-backtrace-limit=0 -fdiagnostics-color=always -ftemplate-depth=1024 -Wpessimizing-move
        $<$<CONFIG:Release>:-freciprocal-math -ffp-contract=fast> # these math optimizations should be OK to use.
        $<$<CONFIG:RelWithDebInfo>:-pedantic -Wall -Wextra -Wfatal-errors -Werror=return-type -Wmissing-field-initializers -Wno-unused-but-set-variable
            -Wno-unused-private-field -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-statement-expression>
        $<$<CONFIG:Debug>:-Wall -Wextra -Wuninitialized -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter -Wno-reorder -Wno-unused-private-field -Wmissing-field-initializers>)
    if(${CMAKE_CXX_COMPILER} MATCHES "templight")
        message("-- Using \"templight\" compiler flags")
        target_compile_options(cxxflags -Xtemplight -profiler -Xtemplight -ignore-system)
    endif()

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message("-- Using \"GNU\" compiler flags")
    target_compile_options(cxxflags INTERFACE -fpermissive -fdiagnostics-color=always -ftemplate-depth=1024)
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        message("-- Using \"MINGW\" compiler flags")
        set(exeflags "-Wl,--stack,16777216")
        target_compile_options(cxxflags INTERFACE -Wa,-mbig-obj)
    endif()

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    message("-- Using \"Intel\" compiler flags")
    target_compile_options(cxxflags INTERFACE -wd173 -wd3373 -wd780 -wd885 -wd673 -wd672 -wd437 -wd3092)
endif()

################################################################################

add_library(libnupack STATIC ${LIBNUPACK_FILES})
add_library(nupack::core ALIAS libnupack)
target_compile_definitions(libnupack PUBLIC $<$<CONFIG:Debug>:NUPACK_DEBUG=2> $<$<CONFIG:RelWithDebInfo>:NUPACK_DEBUG=1>)
target_compile_features(libnupack PUBLIC cxx_std_17)

if(NUPACK_SIMD)
    message("-- Using SIMD instructions for dynamic programs")
else()
    message("-- Not using SIMD instructions for dynamic programs")
    target_compile_definitions(libnupack PUBLIC NUPACK_NO_SIMD)
endif()

################################################################################

find_package(Protobuf CONFIG REQUIRED)
protobuf_generate(LANGUAGE cpp TARGET libnupack PROTOS proto/public.proto PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/include/nupack/)
target_include_directories(libnupack PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include/nupack)

################################################################################

if(NUPACK_SIMD)
    message("-- Using SIMD instructions for dynamic programs")
else()
    message("-- Not using SIMD instructions for dynamic programs")
    target_compile_definitions(libnupack PUBLIC NUPACK_NO_SIMD)
endif()

# This variable goes into Version.h
if(NUPACK_DETERMINISTIC)
    message("-- Using fixed seed for random number generator")
    set(NUPACK_RANDOM_DEVICE 0)
else()
    message("-- Using random seed for random number generator")
    set(NUPACK_RANDOM_DEVICE 1)
endif()

# This variable goes into Version.h
if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    message("-- Not including debug information")
    set(NUPACK_DEBUG_INFO 0)
else()
    message("-- Including debug information")
    set(NUPACK_DEBUG_INFO 1)
endif()

# Version.h is configured to hold some CMake variables including the version number and other things
set(NUPACK_DATA_DIR ${CMAKE_SOURCE_DIR}/data)# CACHE PATH "Path to data files for testing")
set(NUPACK_PARAMETERS_DIR ${SOURCE_DIR}/parameters CACHE PATH "Path to parameter file directory")
configure_file(${SOURCE_DIR}/include/nupack/Version.h.in ${BUILD_DIR}/include/nupack/Version.h)

################################################################################

target_link_libraries(libnupack PRIVATE backward cxxflags)

target_link_libraries(libnupack PUBLIC json TBB::tbb nupack_lapack fmt::fmt nupack-solver Taskflow::Taskflow
nupack-armadillo nupack-boost spdlog::spdlog nupack-simd yaml-cpp::yaml-cpp protobuf::libprotobuf magic_enum::magic_enum)

target_compile_features(libnupack PUBLIC cxx_auto_type cxx_thread_local)
set_target_properties(libnupack PROPERTIES OUTPUT_NAME "nupack")
nupack_cxx_target(libnupack)

target_include_directories(libnupack PUBLIC ${SOURCE_DIR}/include ${BUILD_DIR}/include)

install(TARGETS libnupack ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${SOURCE_DIR}/nupack DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "nupack/*.h")
install(DIRECTORY ${SOURCE_DIR}/nupack DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "nupack/*/*.h")
install(FILES ${BUILD_DIR}/include/nupack/Version.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nupack)
install(DIRECTORY ${SOURCE_DIR}/parameters DESTINATION ${CMAKE_INSTALL_DATADIR}/nupack)

################################################################################

message("-- Building static NUPACK binding library")

add_library(nupack-bind-objects OBJECT ${NUPACK_MODULE_FILES})
target_link_libraries(nupack-bind-objects PUBLIC cxxflags libnupack new_design librebind)
nupack_cxx_target(nupack-bind-objects)

add_library(nupack-bind STATIC ${CMAKE_CURRENT_SOURCE_DIR}/bind/Document.cc)
target_link_libraries(nupack-bind PUBLIC nupack-bind-objects)
target_include_directories(nupack-bind PUBLIC ${SOURCE_DIR})
add_library(nupack::bind ALIAS nupack-bind)
nupack_cxx_target(nupack-bind)
install(TARGETS nupack-bind DESTINATION ${CMAKE_INSTALL_LIBDIR})

################################################################################
# New design code test executable
################################################################################

set(design_lib_source SequenceAdapter.cc ThermoWrapper.cc Complex.cc Defect.cc
    Tube.cc Design.cc Designer.cc Decomposition.cc Split.cc
    Specification.cc Result.cc OutputResult.cc Granularity.cc DesignComponents.cc Models.cc
    Objectives.cc Weights.cc Constraints.cc Proto.cc)
prefix_transform(design_lib_source "source/design" ${design_lib_source})

set(old_design_files constraint_handler.cc sequence_utils.cc)
prefix_transform(old_design_files "source/custom_csp" ${old_design_files})

set(design_lib_source ${design_lib_source} ${old_design_files})

add_library(new_design STATIC ${design_lib_source})
add_library(nupack::design ALIAS new_design)
target_link_libraries(new_design PUBLIC libnupack nupack-gecode)
target_link_libraries(new_design PRIVATE cxxflags)
set_target_properties(new_design PROPERTIES OUTPUT_NAME "nupack-design")
install(TARGETS new_design DESTINATION ${CMAKE_INSTALL_LIBDIR} OPTIONAL)
nupack_cxx_target(new_design)

################################################################################
