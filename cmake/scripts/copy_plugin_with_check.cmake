# This script copies a plugin DLL/shared library to a destination directory
# and emits clear error messages if the copy fails.

if(NOT DEFINED PLUGIN_NAME)
    message(FATAL_ERROR "copy_plugin_with_check.cmake: PLUGIN_NAME is not defined")
endif()
if(NOT DEFINED PLUGIN_SRC)
    message(FATAL_ERROR "copy_plugin_with_check.cmake: PLUGIN_SRC is not defined")
endif()
if(NOT DEFINED PLUGIN_DST)
    message(FATAL_ERROR "copy_plugin_with_check.cmake: PLUGIN_DST is not defined")
endif()

if(NOT EXISTS "${PLUGIN_SRC}")
    message(FATAL_ERROR "Failed to copy plugin ${PLUGIN_NAME}: source file '${PLUGIN_SRC}' does not exist")
endif()

get_filename_component(_plugin_dst_dir "${PLUGIN_DST}" DIRECTORY)
if(NOT IS_DIRECTORY "${_plugin_dst_dir}")
    file(MAKE_DIRECTORY "${_plugin_dst_dir}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PLUGIN_SRC}" "${PLUGIN_DST}"
    RESULT_VARIABLE _copy_result
    OUTPUT_VARIABLE _copy_stdout
    ERROR_VARIABLE _copy_stderr
)

if(NOT _copy_result EQUAL 0)
    message(FATAL_ERROR "Failed to copy plugin ${PLUGIN_NAME}: ${_copy_stderr}")
endif()

if(NOT EXISTS "${PLUGIN_DST}")
    message(FATAL_ERROR "Failed to copy plugin ${PLUGIN_NAME}: destination file '${PLUGIN_DST}' was not created")
endif()

message(STATUS "Plugin ${PLUGIN_NAME} copied to ${PLUGIN_DST}")
