# ============================================================================
# PluginMacros.cmake - platform module descriptor helpers
# ============================================================================

set_property(GLOBAL PROPERTY MEDICALPRO_PLATFORM_DESCRIPTOR_FILES "")
set_property(GLOBAL PROPERTY MEDICALPRO_PLATFORM_DESCRIPTOR_RUNTIME_CONFIGURED FALSE)

function(register_platform_descriptor plugin_name descriptor_source)
    if(NOT EXISTS "${descriptor_source}")
        return()
    endif()

    get_property(_descriptor_files GLOBAL PROPERTY MEDICALPRO_PLATFORM_DESCRIPTOR_FILES)
    set(_descriptor_entry "${plugin_name}|${descriptor_source}")

    if(_descriptor_files STREQUAL "")
        set(_descriptor_files "${_descriptor_entry}")
    else()
        list(APPEND _descriptor_files "${_descriptor_entry}")
        list(REMOVE_DUPLICATES _descriptor_files)
    endif()

    set_property(GLOBAL PROPERTY MEDICALPRO_PLATFORM_DESCRIPTOR_FILES "${_descriptor_files}")
endfunction()

function(configure_platform_descriptor_runtime_copy target_name)
    if(NOT TARGET ${target_name})
        message(WARNING "Target '${target_name}' not found when configuring platform descriptor runtime copy")
        return()
    endif()

    get_property(_configured GLOBAL PROPERTY MEDICALPRO_PLATFORM_DESCRIPTOR_RUNTIME_CONFIGURED)
    if(_configured)
        return()
    endif()

    get_property(_descriptor_files GLOBAL PROPERTY MEDICALPRO_PLATFORM_DESCRIPTOR_FILES)
    if(_descriptor_files STREQUAL "")
        set_property(GLOBAL PROPERTY MEDICALPRO_PLATFORM_DESCRIPTOR_RUNTIME_CONFIGURED TRUE)
        return()
    endif()

    foreach(_descriptor_entry ${_descriptor_files})
        string(REPLACE "|" ";" _descriptor_parts "${_descriptor_entry}")
        list(GET _descriptor_parts 0 _plugin_name)
        list(GET _descriptor_parts 1 _descriptor_source)

        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/plugins/descriptors"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_descriptor_source}"
                "$<TARGET_FILE_DIR:${target_name}>/plugins/descriptors/${_plugin_name}.json"
            COMMENT "Copying ${_plugin_name} platform descriptor to ${target_name} runtime directory"
            VERBATIM
        )
    endforeach()

    set_property(GLOBAL PROPERTY MEDICALPRO_PLATFORM_DESCRIPTOR_RUNTIME_CONFIGURED TRUE)
endfunction()
