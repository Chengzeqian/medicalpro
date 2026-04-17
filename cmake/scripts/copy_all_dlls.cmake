# Copy all *.dll files from a source directory to a destination directory.
# Usage:
#   cmake -Dsrc=<source_dir> -Ddst=<destination_dir> -P copy_all_dlls.cmake
#
# This script is intended to be called from add_custom_command so that we avoid
# extremely long command lines when copying a large number of DLLs (for example
# all runtime DLLs from a Slicer build tree).

if(NOT DEFINED src OR NOT DEFINED dst)
  message(FATAL_ERROR "copy_all_dlls.cmake expects -Dsrc and -Ddst")
endif()

file(GLOB _dlls "${src}/*.dll")
if(NOT _dlls)
  message(STATUS "copy_all_dlls.cmake: no DLLs found in ${src}")
  return()
endif()

foreach(_dll IN LISTS _dlls)
  file(COPY "${_dll}" DESTINATION "${dst}")
endforeach()

message(STATUS "copy_all_dlls.cmake: copied ${_dlls} to ${dst}")
