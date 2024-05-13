vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ww-rm/gecode
    REF b77d22e4c6b3b6449e4e37cb1be2c16b269f4d39
    SHA512 c2d872de23ff5e804168969ee86fecf9eb91e800414883052e8fb4d00d2bdbe4fe1112c22b8d1f5b6ea8abc334ec910ed232d26fb0dc66459b40e3b9b93e9309
    HEAD_REF master
)

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
    # OPTIONS
)

vcpkg_install_cmake()

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)
file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/share)

vcpkg_copy_pdbs()

file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
