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

QString GamerProfileHub::profilePath() const {
    return sharedRoot() + "/profile.json";
}

QString GamerProfileHub::playtimePath() const {
    return sharedRoot() + "/playtime.json";
}

QString GamerProfileHub::gamerpicsDir() const {
    return sharedRoot() + "/gamerpics";
}

QString GamerProfileHub::achievementsDir() const {
    return sharedRoot() + "/achievements";
}

bool GamerProfileHub::profileExists() const {
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
    // XUID is stored as a string to avoid qint64 sign truncation for large values
    m_profile.xuid = obj["xuid"].toString().toULongLong(nullptr, 0);
    if (m_profile.xuid == 0)
        m_profile.xuid = obj["xuid"].toVariant().toULongLong();  // legacy numeric fallback
    m_profile.gamerpicPath = obj["gamerpic_path"].toString();
    m_profile.totalGamerscore = obj["total_gamerscore"].toInt(0);
    m_profile.createdTime = obj["created_time"].toVariant().toLongLong();

    // Load playtime
    m_profile.playtime = loadAllPlaytime();

    return true;
}

bool GamerProfileHub::save() const {
    QDir().mkpath(sharedRoot());

    QJsonObject obj;
    obj["gamertag"] = m_profile.gamertag;
    obj["xuid"] = QStringLiteral("0x") + QString::number(m_profile.xuid, 16).toUpper().rightJustified(16, '0');  // store as hex string to preserve full uint64
    obj["gamerpic_path"] = m_profile.gamerpicPath;
    obj["total_gamerscore"] = static_cast<int>(m_profile.totalGamerscore);
    obj["created_time"] = m_profile.createdTime;

    QFile f(profilePath());
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

bool GamerProfileHub::createProfile(const QString& gamertag, const QString& gamerpicPath) {
    QDir().mkpath(sharedRoot());
    QDir().mkpath(gamerpicsDir());
    QDir().mkpath(achievementsDir());

    m_profile.gamertag = gamertag;
    m_profile.gamerpicPath = gamerpicPath;
    // Generate XUID with 0xB13E prefix
    quint64 rand = QRandomGenerator::global()->generate64() & 0x0000FFFFFFFFFFFFULL;
    m_profile.xuid = 0xB13E000000000000ULL | rand;
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

    // Built-in gamerpics from Qt resources (always available)
    QDir resDir(":/gamerpics/gamerpics");
    for (const auto& entry : resDir.entryInfoList({"*.png", "*.jpg", "*.webp"}, QDir::Files)) {
        pics << entry.absoluteFilePath();
    }

    // User gamerpics dir (custom / user-added)
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
