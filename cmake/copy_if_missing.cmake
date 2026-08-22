if(NOT DEFINED SRC OR NOT DEFINED DEST)
    message(FATAL_ERROR "copy_if_missing.cmake requires SRC and DEST")
endif()

if(NOT EXISTS "${DEST}")
    get_filename_component(dest_dir "${DEST}" DIRECTORY)
    file(MAKE_DIRECTORY "${dest_dir}")
    file(COPY_FILE "${SRC}" "${DEST}")
endif()
