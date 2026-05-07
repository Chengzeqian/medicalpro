#pragma once

#include "UI/NewPages/Navigation/navigation_workspace_types.h"

#include <QString>

class NavigationWorkspaceSnapshotStore
{
public:
    explicit NavigationWorkspaceSnapshotStore(const QString& caseRoot);

    bool persistSnapshot(const NavigationWorkspaceSnapshot& snapshot) const;
    NavigationWorkspaceSnapshot loadSnapshot() const;
    QString latestSnapshotPath() const;

private:
    QString snapshotPath() const;

    QString m_caseRoot;
};
