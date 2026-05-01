#include <QtTest/QtTest>

#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QWidget>

#include "Framework/VTK/embedded_vtk_view_host.h"
#include "UI/NewPages/Navigation/navigation_vtk_bridge.h"

class NavigationVtkBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void bridge_swaps_navigation_host_content_without_page_layout_code();
};

void NavigationVtkBridgeTest::bridge_swaps_navigation_host_content_without_page_layout_code()
{
    QWidget parent;

    auto* planningFrame = new QFrame(&parent);
    auto* planningLayout = new QVBoxLayout(planningFrame);

    auto* navigationFrame = new QFrame(&parent);
    auto* navigationLayout = new QGridLayout(navigationFrame);

    EmbeddedVtkViewHost planningHost(planningFrame, planningLayout);
    EmbeddedVtkViewHost navigationHost(
        navigationFrame,
        navigationLayout,
        nullptr,
        EmbeddedVtkViewHostOptions {
            .hideExistingWidgets = true,
            .gridRow = 0,
            .gridColumn = 0,
            .gridRowSpan = 2,
            .gridColumnSpan = 2
        });

    NavigationVtkBridge bridge(
        &planningHost,
        &navigationHost,
        nullptr,
        []() { return nullptr; },
        []() { return nullptr; });

    QWidget fourViewWidget;
    QWidget navigationViewWidget;

    bridge.showNavigationContent(&fourViewWidget);

    QVERIFY(navigationLayout->indexOf(&fourViewWidget) >= 0);
    QVERIFY(!fourViewWidget.isHidden());

    bridge.showNavigationContent(&navigationViewWidget);

    QVERIFY(navigationLayout->indexOf(&navigationViewWidget) >= 0);
    QVERIFY(!navigationViewWidget.isHidden());
    QVERIFY(fourViewWidget.isHidden());
}

QTEST_MAIN(NavigationVtkBridgeTest)
#include "NavigationVtkBridgeTest.moc"
