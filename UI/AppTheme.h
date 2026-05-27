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
        { "${PAGE_BG_START}", "#060c12" },
        { "${PAGE_BG_END}", "#0c1822" },
        { "${PAGE_BG_ACCENT}", "#102935" },
        { "${SURFACE_SOFT}", "rgba(10, 19, 27, 0.84)" },
        { "${SURFACE_CARD}", "rgba(14, 27, 36, 0.90)" },
        { "${SURFACE_CARD_HOVER}", "rgba(18, 36, 47, 0.98)" },
        { "${SURFACE_PANEL}", "rgba(7, 15, 22, 0.78)" },
        { "${SURFACE_ELEVATED}", "rgba(12, 26, 34, 0.96)" },
        { "${BORDER_SOFT}", "rgba(113, 146, 164, 0.18)" },
        { "${BORDER_STRONG}", "rgba(113, 146, 164, 0.34)" },
        { "${TEXT_PRIMARY}", "#edf4f7" },
        { "${TEXT_SECONDARY}", "#adc0c8" },
        { "${TEXT_MUTED}", "#788d96" },
        { "${ACCENT_PRIMARY}", "#3fb7c8" },
        { "${ACCENT_PRIMARY_HOVER}", "#54c8d8" },
        { "${ACCENT_SECONDARY}", "#47d1bd" },
        { "${WELCOME_PRIMARY}", "#3fb7c8" },
        { "${WELCOME_AMBER}", "#d6a642" },
        { "${STATUS_OK_BG}", "rgba(71, 184, 129, 0.15)" },
        { "${STATUS_OK_BORDER}", "rgba(71, 184, 129, 0.30)" },
        { "${STATUS_WARNING_BG}", "rgba(214, 166, 66, 0.15)" },
        { "${STATUS_WARNING_BORDER}", "rgba(214, 166, 66, 0.30)" },
        { "${STATUS_DANGER_BG}", "rgba(217, 108, 108, 0.15)" },
        { "${STATUS_DANGER_BORDER}", "rgba(217, 108, 108, 0.30)" },
        { "${SUCCESS}", "#47b881" },
        { "${WARNING}", "#d6a642" },
        { "${DANGER}", "#d96c6c" },
        { "${SHADOW}", "rgba(0, 0, 0, 0.32)" }
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
