#include "Framework/Platform/Bootstrap/platform_built_in_module_bootstrap.h"

#include "Framework/Platform/Kernel/platform_plugin_host.h"

#ifdef MEDICALPRO_HAS_USER_MANAGEMENT_PLATFORM_MODULE
#include "Plugins/UserManagement/user_management_module.h"
#endif
#ifdef MEDICALPRO_HAS_DICOM_VIEWER_PLATFORM_MODULE
#include "Plugins/DicomViewer/dicom_viewer_module.h"
#endif
#ifdef MEDICALPRO_HAS_FOUR_VIEW_DISPLAY_PLATFORM_MODULE
#include "Plugins/FourViewDisplay/four_view_display_module.h"
#endif
#ifdef MEDICALPRO_HAS_REGISTRATION_CORE_PLATFORM_MODULE
#include "Plugins/RegistrationCore/registration_core_module.h"
#endif
#ifdef MEDICALPRO_HAS_REGISTRATION_2D3D_PLATFORM_MODULE
#include "Plugins/Registration2D3D/registration_2d3d_module.h"
#endif
#ifdef MEDICALPRO_HAS_OPTICAL_TRACKING_PLATFORM_MODULE
#include "Plugins/OpticalTracking/optical_tracking_module.h"
#endif
#ifdef MEDICALPRO_HAS_OPTICAL_REGISTRATION_PLATFORM_MODULE
#include "Plugins/OpticalRegistration/optical_registration_module.h"
#endif
#ifdef MEDICALPRO_HAS_POINT_REGISTRATION_PLATFORM_MODULE
#include "Plugins/PointRegistration/point_registration_module.h"
#endif
#ifdef MEDICALPRO_HAS_INSTRUMENT_MANAGEMENT_PLATFORM_MODULE
#include "Plugins/InstrumentManagement/instrument_management_module.h"
#endif
#ifdef MEDICALPRO_HAS_BONE_SEGMENTATION_PLATFORM_MODULE
#include "Plugins/BoneSegmentation/bone_segmentation_module.h"
#endif

#include <memory>

void registerBuiltInPlatformModules()
{
    auto& host = PlatformPluginHost::sharedInstance();

#ifdef MEDICALPRO_HAS_USER_MANAGEMENT_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("UserManagement"))) {
        host.registerActivator(std::make_unique<UserManagementModule>());
    }
#endif
#ifdef MEDICALPRO_HAS_DICOM_VIEWER_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("DicomViewer"))) {
        host.registerActivator(std::make_unique<DicomViewerModule>());
    }
#endif
#ifdef MEDICALPRO_HAS_FOUR_VIEW_DISPLAY_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("FourViewDisplay"))) {
        host.registerActivator(std::make_unique<FourViewDisplayModule>());
    }
#endif
#ifdef MEDICALPRO_HAS_REGISTRATION_CORE_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("RegistrationCore"))) {
        host.registerActivator(std::make_unique<RegistrationCoreModule>());
    }
#endif
#ifdef MEDICALPRO_HAS_REGISTRATION_2D3D_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("Registration2D3D"))) {
        host.registerActivator(std::make_unique<Registration2D3DModule>());
    }
#endif
#ifdef MEDICALPRO_HAS_OPTICAL_TRACKING_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("OpticalTracking"))) {
        host.registerActivator(std::make_unique<OpticalTrackingModule>());
    }
#endif
#ifdef MEDICALPRO_HAS_OPTICAL_REGISTRATION_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("OpticalRegistration"))) {
        host.registerActivator(std::make_unique<OpticalRegistrationModule>());
    }
#endif
#ifdef MEDICALPRO_HAS_POINT_REGISTRATION_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("PointRegistration"))) {
        host.registerActivator(std::make_unique<PointRegistrationModule>());
    }
#endif
#ifdef MEDICALPRO_HAS_INSTRUMENT_MANAGEMENT_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("InstrumentManagement"))) {
        host.registerActivator(std::make_unique<InstrumentManagementModule>());
    }
#endif
#ifdef MEDICALPRO_HAS_BONE_SEGMENTATION_PLATFORM_MODULE
    if (!host.hasActivator(QStringLiteral("BoneSegmentation"))) {
        host.registerActivator(std::make_unique<BoneSegmentationModule>());
    }
#endif
}
