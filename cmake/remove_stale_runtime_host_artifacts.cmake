if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

file(REMOVE
    "${OUTPUT_DIR}/CTKPluginFramework.dll"
    "${OUTPUT_DIR}/plugins/liborg_commontk_eventadmin.dll"
)

file(GLOB _legacy_runtime_host_dlls
    "${OUTPUT_DIR}/CTK*.dll"
)

if(_legacy_runtime_host_dlls)
    file(REMOVE ${_legacy_runtime_host_dlls})
endif()
