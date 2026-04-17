#ifndef UI_APPTHEME_H
#define UI_APPTHEME_H

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QMap>

namespace AppTheme
{

inline QMap<QString, QString> threePageTokens()
{
    return {
        { "${PAGE_BG_START}", "#081421" },
        { "${PAGE_BG_END}", "#10243b" },
        { "${PAGE_BG_ACCENT}", "#1a3657" },
        { "${SURFACE_SOFT}", "rgba(9, 19, 31, 0.78)" },
        { "${SURFACE_CARD}", "rgba(13, 27, 42, 0.88)" },
        { "${SURFACE_CARD_HOVER}", "rgba(18, 38, 58, 0.96)" },
        { "${SURFACE_PANEL}", "rgba(7, 16, 27, 0.72)" },
        { "${SURFACE_ELEVATED}", "rgba(10, 22, 34, 0.94)" },
        { "${BORDER_SOFT}", "rgba(124, 160, 191, 0.18)" },
        { "${BORDER_STRONG}", "rgba(124, 160, 191, 0.32)" },
        { "${TEXT_PRIMARY}", "#f4f8fc" },
        { "${TEXT_SECONDARY}", "#b4c4d6" },
        { "${TEXT_MUTED}", "#7f92a8" },
        { "${ACCENT_PRIMARY}", "#ff7a59" },
        { "${ACCENT_PRIMARY_HOVER}", "#ff936f" },
        { "${ACCENT_SECONDARY}", "#5bb6ff" },
        { "${WELCOME_PRIMARY}", "#38C6D9" },
        { "${WELCOME_AMBER}", "#F3A53B" },
        { "${STATUS_OK_BG}", "rgba(61, 220, 151, 0.16)" },
        { "${STATUS_OK_BORDER}", "rgba(61, 220, 151, 0.28)" },
        { "${STATUS_WARNING_BG}", "rgba(243, 165, 59, 0.16)" },
        { "${STATUS_WARNING_BORDER}", "rgba(243, 165, 59, 0.28)" },
        { "${STATUS_DANGER_BG}", "rgba(255, 107, 107, 0.16)" },
        { "${STATUS_DANGER_BORDER}", "rgba(255, 107, 107, 0.28)" },
        { "${SUCCESS}", "#3ddc97" },
        { "${WARNING}", "#ffbf69" },
        { "${DANGER}", "#ff6b6b" },
        { "${SHADOW}", "rgba(0, 0, 0, 0.24)" }
    };
}

inline QString resolveTokens(QString styleSheet)
{
    const auto tokens = threePageTokens();
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
        styleSheet.replace(it.key(), it.value());
    }
    return styleSheet;
}

inline void applyThreePageTheme(QApplication& app)
{
    QFile file(":/UI/styles/three_pages_theme.qss");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[AppTheme] Failed to open theme resource:" << file.fileName();
        return;
    }

    const QString themedStyleSheet = resolveTokens(QString::fromUtf8(file.readAll()));
    if (themedStyleSheet.trimmed().isEmpty()) {
        qWarning() << "[AppTheme] Theme stylesheet is empty";
        return;
    }

    app.setStyleSheet(app.styleSheet() + "\n" + themedStyleSheet);
    qDebug() << "[AppTheme] Three-page theme applied";
}

} // namespace AppTheme

#endif // UI_APPTHEME_H
