#include <QtTest/QtTest>

#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QWidget>

#include "Framework/Navigation/ankle_navigation_types.h"
#include "Framework/VTK/embedded_vtk_view_host.h"
#include "UI/NewPages/Navigation/navigation_vtk_bridge.h"
#include "UI/Widgets/Navigation3DViewWidget.h"

class NavigationVtkBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void bridge_swaps_navigation_host_content_without_page_layout_code();
    void bridge_updates_target_region_overlay_and_risk_tone();
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

void NavigationVtkBridgeTest::bridge_updates_target_region_overlay_and_risk_tone()
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
            .gridRowSpan = 1,
            .gridColumnSpan = 1
        });

    NavigationVtkBridge bridge(
        &planningHost,
        &navigationHost,
        nullptr,
        []() { return nullptr; },
        []() { return nullptr; });

    Navigation3DViewWidget navigationView;
    bridge.showNavigationContent(&navigationView);

    DigitalTwinTargetRegionDefinition targetRegion;
    targetRegion.available = true;
    targetRegion.centerPatient = QVector3D(10.0f, 5.0f, 2.0f);
    targetRegion.radiusMm = 6.0;

    bridge.setTargetRegionDefinition(targetRegion);
    bridge.setTargetRegionRiskTone(QStringLiteral("warning"));

    QVERIFY(bridge.hasTargetRegionDefinition());
    QCOMPARE(bridge.targetRegionDefinition().centerPatient, QVector3D(10.0f, 5.0f, 2.0f));
    QCOMPARE(bridge.targetRegionRiskTone(), QStringLiteral("warning"));
    QVERIFY(navigationView.hasTargetRegionActor());
    QCOMPARE(navigationView.targetRegionRiskTone(), QStringLiteral("warning"));
}

QTEST_MAIN(NavigationVtkBridgeTest)
#include "NavigationVtkBridgeTest.moc"
