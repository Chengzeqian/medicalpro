#pragma once

#include <QtCore/qhashfunctions.h>

enum class AnkleWorkflowStage
{
    Preparation,
    Planning,
    Registration,
    Navigation,
    Evaluation
};

inline uint qHash(AnkleWorkflowStage stage, uint seed = 0) noexcept
{
    return ::qHash(static_cast<uint>(stage), seed);
}
