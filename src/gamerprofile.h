#pragma once

#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>

struct HubAchievement {
    uint16_t id;
    QString name;
    QString description;
    uint16_t gamerscore;
    qint64 unlocked_time;  // 0 if locked
    QString image_path;

    bool isUnlocked() const { return unlocked_time != 0; }
};

struct HubPlaytime {
    uint32_t title_id;
    QString title_name;
    uint64_t total_seconds;
    qint64 last_played;

    QString formattedTime() const {
        uint64_t h = total_seconds / 3600;
        uint64_t m = (total_seconds % 3600) / 60;
        if (h > 0) return QString("%1h %2m").arg(h).arg(m);
        if (m > 0) return QString("%1m").arg(m);
        return "< 1m";
    }
};

struct HubGamerProfile {
    QString gamertag;
    QString gamerpicPath;
    quint64 xuid;
    uint32_t totalGamerscore;
    qint64 createdTime;

    QList<HubPlaytime> playtime;
};

struct HubProfileEntry {
    quint64 xuid;
    QString gamertag;
};

class GamerProfileHub {
public:
    static GamerProfileHub& instance();

    QString sharedRoot() const;
    QString profileRoot() const;  // per-XUID: sharedRoot()/profiles/{XUID}/
    QString profilePath() const;
    QString playtimePath() const;
    QString gamerpicsDir() const;
    QString achievementsDir() const;

    void setActiveXuid(quint64 xuid);
    quint64 activeXuid() const { return m_activeXuid; }

    QList<HubProfileEntry> enumerateProfiles() const;
    void saveProfileIndex() const;
    void migrateLegacyData();

    bool profileExists() const;
    bool load();
    bool save() const;
    bool createProfile(const QString& gamertag, const QString& gamerpicPath = {});

    const HubGamerProfile& profile() const { return m_profile; }
    HubGamerProfile& profile() { return m_profile; }

    void setGamertag(const QString& tag);
    void setGamepic(const QString& path);

    QList<HubAchievement> loadAchievements(uint32_t titleId) const;
    QList<HubPlaytime> loadAllPlaytime() const;

    QStringList availableGamepics() const;

private:
    GamerProfileHub() = default;
    HubGamerProfile m_profile;
    quint64 m_activeXuid = 0;
};
