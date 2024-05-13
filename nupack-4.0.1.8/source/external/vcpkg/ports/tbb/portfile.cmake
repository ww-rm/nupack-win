vcpkg_minimum_required(VERSION 2022-10-12) # for ${VERSION}

set(VCPKG_POLICY_MISMATCHED_NUMBER_OF_BINARIES enabled)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ww-rm/oneTBB
    REF a31adbb96ef55a7d8a4ddc3e5008745a5b569784
    SHA512 481583a85d2336dddf5a11511c72ba832b602d6cd641b837d7418150e77915393f360e11b030ae12693aa40a2f5050878ff4f7d0c619ed9e9b9ef4bfe00c5719
    HEAD_REF onetbb_2021
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DTBB_TEST=OFF
        -DTBB_STRICT=OFF
        -DTBB_ENABLE_IPO=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/TBB")
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()

set(arch_suffix "")
if(VCPKG_TARGET_ARCHITECTURE STREQUAL "x86")
  set(arch_suffix "32")
endif()


if(NOT VCPKG_BUILD_TYPE)
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/tbb${arch_suffix}.pc" "-ltbb12" "-ltbb12_debug")
endif()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/share/doc"
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    # These are duplicate libraries provided on Windows -- users should use the tbb12 libraries instead
    "${CURRENT_PACKAGES_DIR}/lib/tbb.lib"
    "${CURRENT_PACKAGES_DIR}/debug/lib/tbb_debug.lib"
)

file(READ "${CURRENT_PACKAGES_DIR}/share/tbb/TBBConfig.cmake" _contents)
file(WRITE "${CURRENT_PACKAGES_DIR}/share/tbb/TBBConfig.cmake" "
include(CMakeFindDependencyMacro)
find_dependency(Threads)
${_contents}")

file(INSTALL "${SOURCE_PATH}/LICENSE.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
