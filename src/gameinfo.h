#pragma once

#include <QString>
#include <QStringList>
#include <QPixmap>
#include <cstdint>

struct GameInfo {
    QString id;            // folder name (e.g. "BanjoKazooieNnB")
    QString name;
    QString description;
    QString githubRepo;
    uint32_t titleId = 0;  // Xbox 360 title ID (from info.json "title_id" hex string)
    QStringList platforms;
    QStringList releasePackets;
    QPixmap boxart;
    QPixmap banner;
    QString localPath;     // path to the game folder in hub-repo cache

    bool supportsPlatform(const QString& platform) const;
    QString releasePacketForPlatform(const QString& platform) const;
};

QString currentPlatformName();

GameInfo loadGameInfo(const QString& gameDir);
