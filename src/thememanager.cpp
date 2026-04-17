#include "thememanager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

static bool applyStyleFromFile(QFile& qss) {
    if (!qss.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
    return true;
}

static QStringList listThemeBaseNames(const QDir& dir) {
    QStringList out;
    for (const auto& entry : dir.entryInfoList({"*.qss"}, QDir::Files)) {
        out << entry.baseName();
    }
    return out;
}

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

QString ThemeManager::themesDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/themes";
}

QStringList ThemeManager::availableThemes() const {
    QStringList themes;

    // Built-in themes (bundled in resources under :/themes)
    QDir resDir(":/themes");
    if (resDir.exists()) {
        for (const auto& name : listThemeBaseNames(resDir))
            themes << name;
    }

    // User themes (stored in config dir)
    QDir userDir(themesDir());
    if (userDir.exists()) {
        for (const auto& name : listThemeBaseNames(userDir))
            themes << name;
    }

    themes.removeDuplicates();

    // Keep our primary themes near the top.
    int lightIdx = themes.indexOf("Light");
    if (lightIdx > 0) {
        themes.removeAt(lightIdx);
        themes.insert(0, "Light");
    }
    int darkIdx = themes.indexOf("Dark");
    if (darkIdx > 1) {
        themes.removeAt(darkIdx);
        themes.insert(1, "Dark");
    }
    return themes;
}

QString ThemeManager::currentTheme() const {
    return m_current;
}

void ThemeManager::applyTheme(const QString& name) {
    // Migrate legacy preference.
    QString resolved = name;
    if (resolved == "Default")
        resolved = "Light";
    if (resolved == "Xbox360Light")
        resolved = "Light";
    if (resolved == "Xbox360")
        resolved = "Dark";

    m_current = resolved;

    bool ok = false;
    // Prefer bundled theme from resources, fall back to user theme folder.
    QFile qss(QString(":/themes/%1.qss").arg(resolved));
    ok = applyStyleFromFile(qss);
    if (!ok) {
        QFile userQss(themesDir() + "/" + resolved + ".qss");
        ok = applyStyleFromFile(userQss);
    }

    if (!ok) {
        m_current = "Light";
        QFile fallback(":/themes/Light.qss");
        applyStyleFromFile(fallback);
    }

    savePreference();
}

void ThemeManager::savePreference() const {
    QSettings s;
    s.setValue("theme", m_current);
}

void ThemeManager::loadPreference() {
    QSettings s;
    // Prefer light Xbox 360 style on first run.
    QString saved = s.value("theme", "Light").toString();
    applyTheme(saved);
}
