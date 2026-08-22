vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO alandtse/CommonLibSSE-NG
    REF v6.6.0
    SHA512 7b46371745d592cd1cec3d2f4cf9e1321dd8805de9895141b93c9dfc06fb82e3b10e8f4fd304342c3446ae3bd6f77ee22848edd1e639866fe6335cc65c8f47b7
    HEAD_REF ng
)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH2
    REPO ValveSoftware/openvr
    REF ebdea152f8aac77e9a6db29682b81d762159df7e
    SHA512 4fb668d933ac5b73eb4e97eb29816176e500a4eaebe2480cd0411c95edfb713d58312036f15db50884a2ef5f4ca44859e108dec2b982af9163cefcfc02531f63
    HEAD_REF master
)

file(GLOB OPENVR_FILES "${SOURCE_PATH2}/*")
file(COPY ${OPENVR_FILES} DESTINATION "${SOURCE_PATH}/extern/openvr")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DSKSE_SUPPORT_XBYAK=ON
        -DSKSE_SUPPORT_PATCH_SAFETY=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME CommonLibSSE CONFIG_PATH lib/cmake/CommonLibSSE)
vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/cmake/CommonLibSSE.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE")
file(INSTALL "${SOURCE_PATH2}/headers/openvr.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(
    INSTALL "${SOURCE_PATH}/COPYING"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
    RENAME copyright
)
