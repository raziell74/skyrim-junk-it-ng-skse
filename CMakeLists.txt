# It's recommended to set a minimum CMake version.
# If you use CMake features from higher versions, update this to match.
cmake_minimum_required(VERSION 3.21)

# Set your project name. This will be the name of your SKSE .dll file.
project(JunkIt VERSION 2.1.1 LANGUAGES CXX)

#

# YOU DO NOT NEED TO EDIT ANYTHING BELOW HERE
#
# Please see the "Project setup" section of README.md
#

# If you're not using a mod manager, you probably want the SKSE plugin to go
# inside of your Skyrim "Data" folder.
#
# To do this automatically, set the `SKYRIM_FOLDER` environment variable
# to the path of your Skyrim Special Edition folder
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

# If you're using Mod Organizer 2 or Vortex, you might want this to go inside
# of your "mods" folder, inside of a subfolder named "<your mod>".
#
# To do this automatically, set the `SKYRIM_MODS_FOLDER` environment variable
# to the path of your "mods" folder
if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/Junk It")
endif()

# Otherwise, you can set OUTPUT_FOLDER to any place you'd like :)
set(OUTPUT_FOLDER "G:/MO2SSE/mods/Junk It")

# Setup your SKSE plugin as an SKSE plugin!
find_package(CommonLibSSE CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
include(cmake/headerlist.cmake)
include(cmake/sourcelist.cmake)
add_commonlibsse_plugin(${PROJECT_NAME}
    NAME "JunkIt"
    USE_ADDRESS_LIBRARY
    SOURCES ${headers} ${sources})
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/__${PROJECT_NAME}Plugin.cpp"
    PROPERTIES HEADER_FILE_ONLY TRUE)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23) # <--- use C++23 standard
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h) # <--- PCH.h is required!
find_path(SkyPromptAPI_INCLUDE_DIRS "SkyPrompt/API.hpp" REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/third_party/SKSEMenuFramework"
    "${SkyPromptAPI_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} PRIVATE JsonCpp::JsonCpp imgui::imgui Microsoft::DirectXTK d3d11 dxgi)

# When your SKSE .dll is compiled, this will automatically copy the .dll into your mods folder.
# Only works if you configure DEPLOY_ROOT above (or set the SKYRIM_MODS_FOLDER environment variable)
if(DEFINED OUTPUT_FOLDER)
    # If you specify an <OUTPUT_FOLDER> (including via environment variables)
    # then we'll copy your mod files into Skyrim or a mod manager for you!

    # Copy the SKSE plugin .dll files into the SKSE/Plugins/ folder
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    set(SRC_CONFIG_FOLDER "${CMAKE_SOURCE_DIR}/config")
    
    set(DIII_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins/DIII")
    set(SRC_DIII_FOLDER "${CMAKE_SOURCE_DIR}/diii")
    
    set(I4_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins/InventoryInjector")
    set(SRC_I4_FOLDER "${CMAKE_SOURCE_DIR}/i4")
    
    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )
    if (EXISTS "${SRC_CONFIG_FOLDER}/JunkIt.ini")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            # COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRC_CONFIG_FOLDER}/JunkIt.ini" "${DLL_FOLDER}/JunkIt.ini.example"
            COMMAND "${CMAKE_COMMAND}"
                "-DSRC=${SRC_CONFIG_FOLDER}/JunkIt.ini"
                "-DDEST=${DLL_FOLDER}/JunkIt.ini"
                -P "${CMAKE_SOURCE_DIR}/cmake/copy_if_missing.cmake"
            VERBATIM
        )
    endif()
    set(SRC_TRANSLATIONS_FOLDER "${CMAKE_SOURCE_DIR}/papyrus/Interface/translations")
    if (EXISTS "${SRC_TRANSLATIONS_FOLDER}")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_FOLDER}/Interface/translations"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${SRC_TRANSLATIONS_FOLDER}" "${OUTPUT_FOLDER}/Interface/translations"
            VERBATIM
        )
    endif()
    set(SRC_SCRIPTS_FOLDER "${CMAKE_SOURCE_DIR}/papyrus/Scripts")
    if (EXISTS "${SRC_SCRIPTS_FOLDER}")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_FOLDER}/Scripts"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${SRC_SCRIPTS_FOLDER}" "${OUTPUT_FOLDER}/Scripts"
            VERBATIM
        )
    endif()
    set(SRC_INTERFACE_JUNKIT_FOLDER "${CMAKE_SOURCE_DIR}/papyrus/Interface/JunkIt")
    if (EXISTS "${SRC_INTERFACE_JUNKIT_FOLDER}")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT_FOLDER}/Interface/JunkIt"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${SRC_INTERFACE_JUNKIT_FOLDER}" "${OUTPUT_FOLDER}/Interface/JunkIt"
            VERBATIM
        )
    endif()
    if (EXISTS "${SRC_DIII_FOLDER}")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${DIII_FOLDER}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRC_DIII_FOLDER}/JunkIt.json" "${DIII_FOLDER}/JunkIt.json"
            VERBATIM
        )
    endif()
    if (EXISTS "${SRC_I4_FOLDER}")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${I4_FOLDER}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRC_I4_FOLDER}/JunkIt.json" "${I4_FOLDER}/JunkIt.json"
            VERBATIM
        )
    endif()

    # Also deploy the MO2 Python plugin.
    set(SRC_MO2_PLUGIN_FOLDER "${CMAKE_SOURCE_DIR}/MO2 Plugin")
    set(MO2_PLUGINS_FOLDER "${OUTPUT_FOLDER}/../../plugins")
    if(EXISTS "${SRC_MO2_PLUGIN_FOLDER}")
        message(STATUS "MO2 plugin output folder: ${MO2_PLUGINS_FOLDER}")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${MO2_PLUGINS_FOLDER}"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${SRC_MO2_PLUGIN_FOLDER}" "${MO2_PLUGINS_FOLDER}"
            VERBATIM
        )
    endif()


    # If you perform a "Debug" build, also copy .pdb file (for debug symbols)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()

    # Snapshot the MO2 mod folder into a Nexus-ready zip (mod files at zip root).
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(RELEASES_DIR "${CMAKE_SOURCE_DIR}/releases")
        set(RELEASE_STAGING "${RELEASES_DIR}/${PROJECT_NAME} v${PROJECT_VERSION}")
        set(RELEASE_ZIP "${RELEASES_DIR}/${PROJECT_NAME} v${PROJECT_VERSION}.zip")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${RELEASES_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${RELEASE_STAGING}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${RELEASE_STAGING}"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${OUTPUT_FOLDER}"
                "${RELEASE_STAGING}"

            # Remove none release files
            COMMAND "${CMAKE_COMMAND}" -E rm -f "${RELEASE_STAGING}/meta.ini"
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${RELEASE_STAGING}/README.md"
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${RELEASE_STAGING}/.git"
            COMMAND "${CMAKE_COMMAND}" -E rm -f "${RELEASE_STAGING}/.gitignore"
            COMMAND "${CMAKE_COMMAND}" -E rm -f "${RELEASE_STAGING}/SKSE/Plugins/JunkIt.ini"
            VERBATIM
        )
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -f "${RELEASE_ZIP}"
            COMMAND "${CMAKE_COMMAND}" -E tar cf "${RELEASE_ZIP}" --format=zip -- .
            WORKING_DIRECTORY "${RELEASE_STAGING}"
            VERBATIM
        )
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${RELEASE_STAGING}"
            VERBATIM
        )
    endif()
endif()
