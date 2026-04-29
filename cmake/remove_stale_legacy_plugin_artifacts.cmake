if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

set(_legacy_plugin_names
    UserManagement
    DicomViewer
    FourViewDisplay
    InstrumentManagement
    OpticalTracking
    Registration2D3D
    PointRegistration
    OpticalRegistration
    BoneSegmentation
    RegistrationCore
)

foreach(_plugin_name IN LISTS _legacy_plugin_names)
    file(REMOVE
        "${OUTPUT_DIR}/plugins/${_plugin_name}.dll"
        "${OUTPUT_DIR}/plugins/${_plugin_name}.lib"
        "${OUTPUT_DIR}/plugins/${_plugin_name}.exp"
        "${OUTPUT_DIR}/plugins/${_plugin_name}.pdb"
        "${OUTPUT_DIR}/plugins/${_plugin_name}.manifest"
    )
endforeach()
