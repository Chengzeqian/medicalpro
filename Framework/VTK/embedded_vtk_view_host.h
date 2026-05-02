#pragma once

#include "Framework/FrameworkExport.h"

#include <QPointer>

class QLabel;
class QLayout;
class QWidget;

struct EmbeddedVtkViewHostOptions
{
    bool hideExistingWidgets = false;
    int gridRow = 0;
    int gridColumn = 0;
    int gridRowSpan = 1;
    int gridColumnSpan = 1;
};

class FRAMEWORK_EXPORT EmbeddedVtkViewHost
{
public:
    EmbeddedVtkViewHost(QWidget* frame,
                        QLayout* layout,
                        QLabel* placeholder = nullptr,
                        EmbeddedVtkViewHostOptions options = {});

    void attach(QWidget* widget);
    void detach();
    void setPaused(bool paused);
    QWidget* widget() const;

private:
    QPointer<QWidget> m_frame;
    QPointer<QLayout> m_layout;
    QPointer<QLabel> m_placeholder;
    QPointer<QWidget> m_widget;
    EmbeddedVtkViewHostOptions m_options;
};
