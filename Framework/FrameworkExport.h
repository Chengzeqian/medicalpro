#ifndef FRAMEWORKEXPORT_H
#define FRAMEWORKEXPORT_H

#include <QtCore/qglobal.h>

/**
 * @brief Framework library export/import macro
 *
 * When building Framework.dll, FRAMEWORK_LIBRARY is defined (see CMakeLists.txt)
 * which causes symbols to be exported with __declspec(dllexport).
 *
 * When other modules (main app, plugins) include this header,
 * FRAMEWORK_LIBRARY is NOT defined, so symbols are imported with __declspec(dllimport).
 *
 * This allows Framework singletons (CTKManager, VTKGlobalInitializer, etc.)
 * to be truly global and shared across all modules in the process.
 */
#ifdef _WIN32
    #ifdef FRAMEWORK_LIBRARY
        #define FRAMEWORK_EXPORT __declspec(dllexport)
    #else
        #define FRAMEWORK_EXPORT __declspec(dllimport)
    #endif
#else
    #define FRAMEWORK_EXPORT __attribute__((visibility("default")))
#endif

#endif // FRAMEWORKEXPORT_H
