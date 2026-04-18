#include "gamerprofile.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QRandomGenerator>

GamerProfileHub& GamerProfileHub::instance() {
    static GamerProfileHub inst;
    return inst;
}

QString GamerProfileHub::sharedRoot() const {
    QString xdg = qEnvironmentVariable("XDG_DATA_HOME");
    if (!xdg.isEmpty())
        return xdg + "/rexglue";
#ifdef Q_OS_WIN
    QString appdata = qEnvironmentVariable("APPDATA");
    if (!appdata.isEmpty())
        return appdata + "/rexglue";
#endif
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/share/rexglue";
}

QString GamerProfileHub::profileRoot() const {
    return sharedRoot() + "/profiles/" + QString::number(m_activeXuid, 16).toUpper().rightJustified(16, '0');
}

QString GamerProfileHub::profilePath() const {
    return profileRoot() + "/profile.json";
}

QString GamerProfileHub::playtimePath() const {
    return profileRoot() + "/playtime.json";
}

QString GamerProfileHub::gamerpicsDir() const {
    return sharedRoot() + "/gamerpics";
}

QString GamerProfileHub::achievementsDir() const {
    return profileRoot() + "/achievements";
}

void GamerProfileHub::setActiveXuid(quint64 xuid) {
    m_activeXuid = xuid;
}

QList<HubProfileEntry> GamerProfileHub::enumerateProfiles() const {
    QList<HubProfileEntry> entries;
    QString indexPath = sharedRoot() + "/profiles.json";

    QFile f(indexPath);
    if (!f.open(QIODevice::ReadOnly))
        return entries;

    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return entries;

    for (const auto& val : doc.array()) {
        auto obj = val.toObject();
        HubProfileEntry pe;
        pe.xuid = obj["xuid"].toString().toULongLong(nullptr, 0);
        pe.gamertag = obj["gamertag"].toString("Player");
        if (pe.xuid != 0)
            entries.append(pe);
    }

    return entries;
}

void GamerProfileHub::saveProfileIndex() const {
    QDir().mkpath(sharedRoot());
    QString indexPath = sharedRoot() + "/profiles.json";

    // Read existing
    QJsonArray arr;
    QFile rf(indexPath);
    if (rf.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(rf.readAll());
        if (doc.isArray()) arr = doc.array();
        rf.close();
    }

    // Update or add our entry
    QString xuidStr = QStringLiteral("0x") + QString::number(m_profile.xuid, 16).toUpper().rightJustified(16, '0');
    bool found = false;
    for (int i = 0; i < arr.size(); ++i) {
        auto obj = arr[i].toObject();
        if (obj["xuid"].toString() == xuidStr) {
            obj["gamertag"] = m_profile.gamertag;
            arr[i] = obj;
            found = true;
            break;
        }
    }
    if (!found) {
        QJsonObject entry;
        entry["xuid"] = xuidStr;
        entry["gamertag"] = m_profile.gamertag;
        arr.append(entry);
    }

    QFile wf(indexPath);
    if (wf.open(QIODevice::WriteOnly))
        wf.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void GamerProfileHub::migrateLegacyData() {
    QString legacyProfile = sharedRoot() + "/profile.json";
    if (!QFile::exists(legacyProfile))
        return;

    QFile f(legacyProfile);
    if (!f.open(QIODevice::ReadOnly))
        return;

    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;

    auto obj = doc.object();
    quint64 xuid = obj["xuid"].toString().toULongLong(nullptr, 0);
    if (xuid == 0)
        xuid = obj["xuid"].toVariant().toULongLong();
    if (xuid == 0) {
        quint64 rand = QRandomGenerator::global()->generate64() & 0x0000FFFFFFFFFFFFULL;
        xuid = 0xB13E000000000000ULL | rand;
    }

    QString destDir = sharedRoot() + "/profiles/" + QString::number(xuid, 16).toUpper().rightJustified(16, '0');
    QDir().mkpath(destDir + "/achievements");

    // Move profile.json
    QFile::rename(legacyProfile, destDir + "/profile.json");

    // Move playtime.json
    QString legacyPlaytime = sharedRoot() + "/playtime.json";
    if (QFile::exists(legacyPlaytime))
        QFile::rename(legacyPlaytime, destDir + "/playtime.json");

    // Move achievements/
    QString legacyAchiev = sharedRoot() + "/achievements";
    QDir achDir(legacyAchiev);
    if (achDir.exists()) {
        for (const auto& entry : achDir.entryInfoList(QDir::Files)) {
            QFile::rename(entry.absoluteFilePath(), destDir + "/achievements/" + entry.fileName());
        }
        achDir.removeRecursively();
    }

    // Create profiles.json
    QJsonArray arr;
    QJsonObject pe;
    pe["xuid"] = QStringLiteral("0x") + QString::number(xuid, 16).toUpper().rightJustified(16, '0');
    pe["gamertag"] = obj["gamertag"].toString("Player");
    arr.append(pe);

    QFile idx(sharedRoot() + "/profiles.json");
    if (idx.open(QIODevice::WriteOnly))
        idx.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

bool GamerProfileHub::profileExists() const {
    if (m_activeXuid == 0) {
        // Check legacy or new index
        return QFile::exists(sharedRoot() + "/profile.json") ||
               QFile::exists(sharedRoot() + "/profiles.json");
    }
    return QFile::exists(profilePath());
}

bool GamerProfileHub::load() {
    QFile f(profilePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;

    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;

    auto obj = doc.object();
    m_profile.gamertag = obj["gamertag"].toString("Player");
    m_profile.xuid = obj["xuid"].toString().toULongLong(nullptr, 0);
    if (m_profile.xuid == 0)
        m_profile.xuid = obj["xuid"].toVariant().toULongLong();
    m_profile.gamerpicPath = obj["gamerpic_path"].toString();
    m_profile.totalGamerscore = obj["total_gamerscore"].toInt(0);
    m_profile.createdTime = obj["created_time"].toVariant().toLongLong();

    m_profile.playtime = loadAllPlaytime();

    return true;
}

bool GamerProfileHub::save() const {
    QDir().mkpath(profileRoot());

    QJsonObject obj;
    obj["gamertag"] = m_profile.gamertag;
    obj["xuid"] = QStringLiteral("0x") + QString::number(m_profile.xuid, 16).toUpper().rightJustified(16, '0');
    obj["gamerpic_path"] = m_profile.gamerpicPath;
    obj["total_gamerscore"] = static_cast<int>(m_profile.totalGamerscore);
    obj["created_time"] = m_profile.createdTime;

    QFile f(profilePath());
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));

    saveProfileIndex();

    return true;
}

bool GamerProfileHub::createProfile(const QString& gamertag, const QString& gamerpicPath) {
    // Reuse active XUID if already set (e.g. the default B13EBABEBABEBABE),
    // otherwise generate a fresh one with the 0xB13E prefix.
    if (m_activeXuid == 0) {
        quint64 rand = QRandomGenerator::global()->generate64() & 0x0000FFFFFFFFFFFFULL;
        m_activeXuid = 0xB13E000000000000ULL | rand;
    }
    m_profile.xuid = m_activeXuid;

    QDir().mkpath(profileRoot());
    QDir().mkpath(gamerpicsDir());
    QDir().mkpath(achievementsDir());

    m_profile.gamertag = gamertag;
    m_profile.gamerpicPath = gamerpicPath;
    m_profile.totalGamerscore = 0;
    m_profile.createdTime = QDateTime::currentSecsSinceEpoch();

    return save();
}

void GamerProfileHub::setGamertag(const QString& tag) {
    m_profile.gamertag = tag;
    save();
}

void GamerProfileHub::setGamepic(const QString& path) {
    if (path.isEmpty()) return;

    QDir().mkpath(gamerpicsDir());
    QString dest = gamerpicsDir() + "/" + QFileInfo(path).fileName();
    if (path != dest)
        QFile::copy(path, dest);
    m_profile.gamerpicPath = dest;
    save();
}

QList<HubAchievement> GamerProfileHub::loadAchievements(uint32_t titleId) const {
    QList<HubAchievement> result;
    QString path = achievementsDir() + "/" + QString("%1.json").arg(titleId, 8, 16, QChar('0')).toUpper();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return result;

    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return result;

    for (const auto& val : doc.array()) {
        auto obj = val.toObject();
        HubAchievement a;
        a.id = obj["id"].toInt(0);
        a.name = obj["name"].toString();
        a.description = obj["description"].toString();
        a.gamerscore = obj["gamerscore"].toInt(0);
        a.unlocked_time = obj["unlocked_time"].toVariant().toLongLong();
        a.image_path = obj["image_path"].toString();
        result.append(a);
    }

    return result;
}

QList<HubPlaytime> GamerProfileHub::loadAllPlaytime() const {
    QList<HubPlaytime> result;

    QFile f(playtimePath());
    if (!f.open(QIODevice::ReadOnly))
        return result;

    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return result;

    auto obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        auto val = it.value().toObject();
        HubPlaytime pt;
        pt.title_id = it.key().toUInt(nullptr, 16);
        pt.title_name = val["title_name"].toString();
        pt.total_seconds = val["total_seconds"].toVariant().toULongLong();
        pt.last_played = val["last_played"].toVariant().toLongLong();
        result.append(pt);
    }

    return result;
}

QStringList GamerProfileHub::availableGamepics() const {
    QStringList pics;

    QDir resDir(":/gamerpics/gamerpics");
    for (const auto& entry : resDir.entryInfoList({"*.png", "*.jpg", "*.webp"}, QDir::Files)) {
        pics << entry.absoluteFilePath();
    }

    QDir dir(gamerpicsDir());
    if (dir.exists()) {
        QStringList builtinNames;
        for (const auto& p : std::as_const(pics))
            builtinNames << QFileInfo(p).fileName();

        for (const auto& entry : dir.entryInfoList({"*.png", "*.jpg", "*.webp"}, QDir::Files)) {
            if (!builtinNames.contains(entry.fileName()))
                pics << entry.absoluteFilePath();
        }
    }

    return pics;
}
