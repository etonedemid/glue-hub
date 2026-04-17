#pragma once

#include <QString>
#include <QStringList>

class ThemeManager {
public:
    static ThemeManager& instance();

    /// Available built-in theme names
    QStringList availableThemes() const;

    /// Current theme name
    QString currentTheme() const;

    /// Apply a theme by name. "Default" loads bundled QSS. Others load from themes dir.
    void applyTheme(const QString& name);

    /// Path to user themes directory
    QString themesDir() const;

    /// Save current theme choice
    void savePreference() const;

    /// Load saved preference
    void loadPreference();

private:
    ThemeManager() = default;
    QString m_current = "Default";
};
