#include <QtTest/QtTest>

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

#include "Framework/VTK/embedded_vtk_view_host.h"

class EmbeddedVtkViewHostTest : public QObject
{
    Q_OBJECT

private slots:
    void host_swaps_placeholder_and_attaches_widget_once();
};

void EmbeddedVtkViewHostTest::host_swaps_placeholder_and_attaches_widget_once()
{
    QWidget parent;
    auto* frame = new QFrame(&parent);
    auto* layout = new QVBoxLayout(frame);
    auto* placeholder = new QLabel(QStringLiteral("placeholder"), frame);
    layout->addWidget(placeholder);

    EmbeddedVtkViewHost host(frame, layout, placeholder);
    QWidget widget;

    host.attach(&widget);

    QCOMPARE(layout->indexOf(&widget) >= 0, true);
    QCOMPARE(placeholder->isHidden(), true);
}

QTEST_MAIN(EmbeddedVtkViewHostTest)
#include "EmbeddedVtkViewHostTest.moc"
