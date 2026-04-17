#include "MedicalDataStructures.h"

#include <QMetaType>

namespace {

struct MetaTypeRegistrar {
    MetaTypeRegistrar()
    {
        qRegisterMetaType<InstrumentItem>("InstrumentItem");
        qRegisterMetaType<MarkerGeometry>("MarkerGeometry");
        qRegisterMetaType<InstrumentStatistics>("InstrumentStatistics");
    }
};

static MetaTypeRegistrar s_registrar;

} // namespace
