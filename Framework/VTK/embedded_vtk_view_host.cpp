#include "Framework/VTK/embedded_vtk_view_host.h"

#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QWidget>

EmbeddedVtkViewHost::EmbeddedVtkViewHost(QWidget* frame,
                                         QLayout* layout,
                                         QLabel* placeholder,
                                         EmbeddedVtkViewHostOptions options)
    : m_frame(frame)
    , m_layout(layout)
    , m_placeholder(placeholder)
    , m_options(options)
{
}

void EmbeddedVtkViewHost::attach(QWidget* widget)
{
    if (!m_layout || !widget) {
        return;
    }

    if (m_options.hideExistingWidgets) {
        for (int i = 0; i < m_layout->count(); ++i) {
            auto* item = m_layout->itemAt(i);
            if (item && item->widget() && item->widget() != widget) {
                item->widget()->hide();
            }
        }
    }

    m_widget = widget;
    if (m_frame && widget->parentWidget() != m_frame) {
        widget->setParent(m_frame);
    }

    if (auto* gridLayout = qobject_cast<QGridLayout*>(m_layout.data())) {
        gridLayout->removeWidget(widget);
        gridLayout->addWidget(widget,
            m_options.gridRow,
            m_options.gridColumn,
            m_options.gridRowSpan,
            m_options.gridColumnSpan);
    } else if (m_layout->indexOf(widget) < 0) {
        m_layout->addWidget(widget);
    }

    if (m_placeholder) {
        m_placeholder->hide();
    }

    widget->show();
}

void EmbeddedVtkViewHost::detach()
{
    if (!m_widget) {
        if (m_placeholder) {
            m_placeholder->show();
        }
        return;
    }

    if (m_layout) {
        m_layout->removeWidget(m_widget);
    }

    m_widget->hide();
    if (m_placeholder) {
        m_placeholder->show();
    }
}

void EmbeddedVtkViewHost::setPaused(bool paused)
{
    if (!m_widget) {
        return;
    }

    if (paused) {
        m_widget->hide();
        if (m_placeholder) {
            m_placeholder->show();
        }
        return;
    }

    if (m_placeholder) {
        m_placeholder->hide();
    }
    m_widget->show();
}

QWidget* EmbeddedVtkViewHost::widget() const
{
    return m_widget;
}
