#include "gameinfo.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSysInfo>

bool GameInfo::supportsPlatform(const QString& platform) const {
    return platforms.contains(platform, Qt::CaseInsensitive);
}

QString GameInfo::releasePacketForPlatform(const QString& platform) const {
    // Map platform to expected extension patterns (case-insensitive)
    for (const auto& pkt : releasePackets) {
        if (platform.compare("Linux", Qt::CaseInsensitive) == 0 &&
            (pkt.contains("Linux", Qt::CaseInsensitive) ||
             pkt.endsWith(".AppImage", Qt::CaseInsensitive) ||
             pkt.endsWith(".appimage", Qt::CaseInsensitive) ||
             pkt.contains("linux", Qt::CaseInsensitive)))
            return pkt;
        if (platform.compare("Windows", Qt::CaseInsensitive) == 0 &&
            (pkt.contains("windows", Qt::CaseInsensitive) ||
             pkt.endsWith(".zip", Qt::CaseInsensitive) ||
             pkt.endsWith(".exe", Qt::CaseInsensitive)))
            return pkt;
        if (platform.compare("Android", Qt::CaseInsensitive) == 0 &&
            (pkt.contains("android", Qt::CaseInsensitive) ||
             pkt.endsWith(".apk", Qt::CaseInsensitive)))
            return pkt;
    }
    return {};
}

QString currentPlatformName() {
#ifdef Q_OS_WIN
    return "Windows";
#elif defined(Q_OS_ANDROID)
    return "Android";
#elif defined(Q_OS_LINUX)
    return "Linux";
#elif defined(Q_OS_MACOS)
    return "macOS";
#else
    return QSysInfo::productType();
#endif
}

GameInfo loadGameInfo(const QString& gameDir) {
    GameInfo info;
    QDir dir(gameDir);
    info.id = dir.dirName();
    info.localPath = gameDir;

    QFile f(dir.filePath("info.json"));
    if (!f.open(QIODevice::ReadOnly))
        return info;

    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return info;

    auto obj = doc.object();
    info.name = obj["name"].toString(info.id);
    info.description = obj["description"].toString();
    info.githubRepo = obj["github_repo"].toString();
    info.titleId = obj["title_id"].toString().toUInt(nullptr, 16);

    for (const auto& v : obj["available_platforms"].toArray())
        info.platforms << v.toString();

    for (const auto& v : obj["release_packets_names"].toArray())
        info.releasePackets << v.toString();

    // Load images
    QString boxartPath = dir.filePath("boxart.webp");
    if (QFile::exists(boxartPath))
        info.boxart.load(boxartPath);

    // Try png/jpg fallbacks
    if (info.boxart.isNull()) {
        for (const auto& ext : {"boxart.png", "boxart.jpg"}) {
            QString p = dir.filePath(ext);
            if (QFile::exists(p)) { info.boxart.load(p); break; }
        }
    }

    QString bannerPath = dir.filePath("banner.webp");
    if (QFile::exists(bannerPath))
        info.banner.load(bannerPath);

    if (info.banner.isNull()) {
        for (const auto& ext : {"banner.png", "banner.jpg"}) {
            QString p = dir.filePath(ext);
            if (QFile::exists(p)) { info.banner.load(p); break; }
        }
    }

    return info;
}
