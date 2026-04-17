#include "DicomDataStructures.h"
#include <QMetaType>

// 注册Qt元类型系统，使得这些结构可以在Qt的信号槽系统中使用
static void registerDicomMetaTypes()
{
    qRegisterMetaType<DicomImageInfo>("DicomImageInfo");
    qRegisterMetaType<DicomSeriesInfo>("DicomSeriesInfo");
    qRegisterMetaType<DicomStudyInfo>("DicomStudyInfo");
    qRegisterMetaType<DicomPatientInfo>("DicomPatientInfo");
    qRegisterMetaType<DicomAnnotation>("DicomAnnotation");
    qRegisterMetaType<DicomDisplayParams>("DicomDisplayParams");
}

// 静态初始化器，确保程序启动时注册元类型
static struct DicomMetaTypeRegistrar {
    DicomMetaTypeRegistrar() {
        registerDicomMetaTypes();
    }
} dicomMetaTypeRegistrar;
