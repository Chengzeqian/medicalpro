#include "PatientDataStructures.h"
#include <QtGlobal>

namespace {
struct RegisterMetaTypes {
    RegisterMetaTypes() {
        qRegisterMetaType<PatientInfo>("PatientInfo");
        qRegisterMetaType<PatientImageInfo>("PatientImageInfo");
        qRegisterMetaType<SurgeryRecord>("SurgeryRecord");
        qRegisterMetaType<PatientSearchCriteria>("PatientSearchCriteria");
    }
};
static RegisterMetaTypes reg;
} // namespace

